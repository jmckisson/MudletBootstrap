#include "MudletBootstrap.h"
#include <QApplication>
#include <QCryptographicHash>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QFile>
#include <QProcess>
#include <QDebug>
#include <QSettings>
#include <QUrl>
#include <QVBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>

QMap<QString, QString> getPlatformFeedMap(const QString &type) {

    const QString dblsqdFeedType = type == "PTB" ? "public-test-build" : "release";

    const QString dblsqdFeedUrl = "https://feeds.dblsqd.com/MKMMR7HNSP65PquQQbiDIw/";

    return {
        {"mac/arm",         QString("%1%2/mac/arm").arg(dblsqdFeedUrl).arg(dblsqdFeedType)},
        {"mac/x86_64",      QString("%1%2/mac/x86_64").arg(dblsqdFeedUrl).arg(dblsqdFeedType)},
        {"win/x86_64",      QString("%1%2/win/x86_64").arg(dblsqdFeedUrl).arg(dblsqdFeedType)},
        {"win/x86",         QString("%1%2/win/x86").arg(dblsqdFeedUrl).arg(dblsqdFeedType)},
        {"linux/x86_64",    QString("%1%2/linux/x86_64").arg(dblsqdFeedUrl).arg(dblsqdFeedType)}
    };
}


QString detectOS() {
    QString osKey;

#if defined(Q_OS_WIN)
    if (sizeof(void*) == 8) {
        osKey = "win/x86_64";
    } else {
        osKey = "win/x86";
    }
#elif defined(Q_OS_MAC)
    QString architecture = QSysInfo::currentCpuArchitecture();
    if (architecture.contains("arm64")) {
        osKey = "mac/arm";
    } else if (architecture.contains("x86_64")) {
        osKey = "mac/x86_64";
    }
#elif defined(Q_OS_LINUX)
    osKey = "linux/x86_64";  // Extend for other architectures as needed
#else
    osKey = "unknown";
#endif

    return osKey;
}


QString readLaunchProfileFromResource() {
    QSettings settings(":/resources/launch.ini", QSettings::IniFormat);

    QString profile = settings.value("Settings/MUDLET_PROFILES", "").toString();

    if (profile.isEmpty()) {
        qDebug() << "MUDLET_PROFILES not found in resource file.";
    }

    return profile;
}


/**
 * @brief 
 * 
 * @param filePath Path to the file of whose hash wil be computed
 * @param expectedHash Expected sha256 hash
 * @return true If the expectedHash matches the sha256 hash of the file
 * @return false If the expectedHash does not match the sha256 hash of the file
 */
bool verifyFileSha256(const QString &filePath, const QString &expectedHash) {

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Failed to open file:" << filePath;
        return false;
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) {
        qDebug() << "Failed to compute SHA-256 hash.";
        return false;
    }

    file.close();

    QString hexHash = hash.result().toHex();

    if (hash.result().toHex().isEmpty()) {
        qDebug() << "SHA-256 computation failed.";
        return false;
    }

    if (hexHash.compare(expectedHash, Qt::CaseInsensitive) == 0) {
        qDebug() << "SHA-256 verification succeeded.";
        return true;
    } else {
        qDebug() << "SHA-256 verification failed.";
        qDebug() << "Computed:" << hexHash;
        qDebug() << "Expected:" << expectedHash;
        return false;
    }
}


MudletBootstrap::MudletBootstrap(QObject *parent) :
    QObject(parent),
    currentReply(nullptr) {

    progressWindow = new QWidget;
    progressWindow->setWindowTitle("Downloading...");
    progressWindow->resize(400, 150);

    QVBoxLayout *layout = new QVBoxLayout(progressWindow);

    statusLabel = new QLabel("Preparing to download...", progressWindow);
    layout->addWidget(statusLabel);

    progressBar = new QProgressBar(progressWindow);
    progressBar->setRange(0, 100);
    layout->addWidget(progressBar);

    progressWindow->setLayout(layout);
}

void MudletBootstrap::start() {
    fetchPlatformFeed();

    // Show the progress bar window
    progressWindow->show();
}

/**
 * @brief Query the platform OS and fetch the proper platform feed from dblsqd
 */
void MudletBootstrap::fetchPlatformFeed() {

    QSettings settings(":/resources/launch.ini", QSettings::IniFormat);

    QString releaseType = settings.value("Settings/RELEASE_TYPE", "").toString();

    QMap<QString, QString> feedMap = getPlatformFeedMap(releaseType);

    QString os = detectOS();

    QString feedUrl = feedMap.value(os);

    if (feedUrl.isEmpty()) {
        qDebug() << "No feed URL found for platform:" << os;
        return;
    }

    currentReply = networkManager.get(QNetworkRequest(QUrl(feedUrl)));

    connect(currentReply, &QNetworkReply::finished, this, &MudletBootstrap::onFetchPlatformFeedFinished);
}

/**
 * @brief Called upon complete receipt of the platform feed. 
 * Extracts the url and sha256 from the JSON and sets up a new download for the proper file.
 */
void MudletBootstrap::onFetchPlatformFeedFinished() {
    if (currentReply->error() != QNetworkReply::NoError) {
        qDebug() << "Error fetching feed:" << currentReply->errorString();
        currentReply->deleteLater();
        return;
    }

    QByteArray jsonData = currentReply->readAll();
    currentReply->deleteLater();

    QJsonDocument doc = QJsonDocument::fromJson(jsonData);
    if (doc.isNull() || !doc.isObject()) {
        qDebug() << "Invalid JSON data.";
        return;
    }

    QJsonObject rootObj = doc.object();
    QJsonArray releases = rootObj.value("releases").toArray();
    if (releases.isEmpty()) {
        qDebug() << "No releases found.";
        return;
    }

    QJsonObject firstRelease = releases[0].toObject();
    QJsonObject download = firstRelease.value("download").toObject();
    info.sha256 = download.value("sha256").toString();
    info.url = download.value("url").toString();

    qDebug() << "SHA-256:" << info.sha256;
    qDebug() << "URL:" << info.url;

    QRegularExpression regex(R"(/([^/]+)\.(exe|dmg|AppImage\.tar)$)");
    QRegularExpressionMatch match = regex.match(info.url);

    if (match.hasMatch()) {
        info.appName = match.captured(1);
    } else {
        qDebug() << "No match found in URL:" << info.url;
        return;
    }

    outputFile = info.appName;

    // Create a request and start downloading the Mudlet installer
    QNetworkRequest request{QUrl(info.url)};
    currentReply = networkManager.get(request);

    connect(currentReply, &QNetworkReply::downloadProgress, this, &MudletBootstrap::onDownloadProgress);
    connect(currentReply, &QNetworkReply::finished, this, &MudletBootstrap::onDownloadFinished);
    connect(currentReply, &QNetworkReply::errorOccurred, this, &MudletBootstrap::onDownloadError);

    statusLabel->setText(QString("Downloading %1...").arg(outputFile));
}


void MudletBootstrap::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal) {
    if (bytesTotal > 0) {
        int progress = static_cast<int>((bytesReceived * 100) / bytesTotal);
        progressBar->setValue(progress);
    }
    statusLabel->setText(QString("Downloading %1... %2 / %3 bytes").arg(info.appName).arg(bytesReceived).arg(bytesTotal));
}


void MudletBootstrap::installApplication(const QString &filePath) {

    QProcess installerProcess;

    // Read the profile from the .ini file
    QString launchProfile = readLaunchProfileFromResource();
    if (launchProfile.isEmpty()) {
        qDebug() << "No launch profile found. Using default.";
    } else {
        // Pass along the launch profile to the environment
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("MUDLET_PROFILES", launchProfile);
        installerProcess.setProcessEnvironment(env);
    }
    

    // Install the application
#if defined(Q_OS_WIN)
    installerProcess.start("cmd.exe", {"/C", outputFile});
#elif defined(Q_OS_MAC)
    installerProcess.start("open", {outputFile});
#elif defined(Q_OS_LINUX)
    installerProcess.start("chmod", {"+x", outputFile}); // Make executable
    installerProcess.waitForFinished();
    installerProcess.start("./" + outputFile);
#endif
    installerProcess.waitForFinished();
    statusLabel->setText("Installation Completed");
    progressWindow->close();
}


/**
 * @brief Verifies the sha256 hash and starts the install process if the hash matches what we got
 * from the dblsqd feed.
 * Called upon completion of the Mudlet installer download.
 * 
 */
void MudletBootstrap::onDownloadFinished() {
    if (currentReply->error() != QNetworkReply::NoError) {
        statusLabel->setText(QString("Error downloading file: %1").arg(currentReply->errorString()));
        return;
    }

    QFile file(outputFile);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(currentReply->readAll());
        file.close();
        qDebug() << "Downloaded to:" << outputFile;

        // Verify the SHA-256 checksum
        if (!verifyFileSha256(outputFile, info.sha256)) {
            qDebug() << "Checksum verification failed. Exiting.";
            statusLabel->setText("SHA256 Verification Failed");
            return;
        }

        statusLabel->setText(QString("Installing %1").arg(info.appName));

        installApplication(outputFile);

    } else {
        qDebug() << "Failed to save file.";
    }

    currentReply->deleteLater();
}

void MudletBootstrap::onDownloadError(QNetworkReply::NetworkError error) {
    qDebug() << "Download error:" << currentReply->errorString();
    currentReply->deleteLater();
}


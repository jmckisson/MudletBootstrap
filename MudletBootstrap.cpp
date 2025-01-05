#include "MudletBootstrap.h"
#include <QApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QFile>
#include <QProcess>
#include <QProcessEnvironment>
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
        QString os = detectOS();
        
        info.appName = match.captured(1);
        if (os.startsWith("mac") || os.startsWith("linux")) {
            info.appName += "." + match.captured(2);
        }
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
    statusLabel->setText(QString("Downloading %1... %2 / %3 MB")
        .arg(info.appName)
        .arg(bytesReceived/1048576.0, 0, 'g', 2)
        .arg(bytesTotal/1048576.0, 0, 'g', 2));
}


void installAndRunDmg(QProcessEnvironment &env, const QString& dmgFilePath) {
    QProcess process;

    // Mount the .dmg file
    QString mountPoint;
    process.start("hdiutil", {"attach", dmgFilePath, "-nobrowse"});
    process.waitForFinished();
    QString output = process.readAllStandardOutput();
    qDebug() << output;

    // Extract the mount point (assumes it's in the last line of the output)
    QStringList lines = output.split('\n');
    for (const QString& line : lines) {
        if (line.contains("/Volumes/")) {
            mountPoint = line.section('\t', -1);
            break;
        }
    }
    if (mountPoint.isEmpty()) {
        qWarning() << "Failed to mount .dmg.";
        return;
    }
    qDebug() << "Mounted at:" << mountPoint;

    // Copy the application to ~/Applications
    QString appName = "Mudlet.app";
    QString appPath = mountPoint + "/" + appName;
    QString targetDir = QDir::homePath() + "/Applications/";
    QString targetAppPath = targetDir + appName;

    if (QFile::exists(targetAppPath)) {
        qDebug() << "Application already exists at" << targetAppPath << ". Removing it...";
        if (!QFile::remove(targetAppPath)) {
            qWarning() << "Failed to remove existing application, trying recursive delete...";
            if (!QDir(targetAppPath).removeRecursively()) {
                qWarning() << "Failed to recursively remove existing application.";
                process.start("rm", {"-rf", targetAppPath});
                process.waitForFinished();
                if (process.exitCode() != 0) {
                    qWarning() << "Failed to remove application:" << process.readAllStandardError();
                    return;
                }
            }
            
        }
        qDebug() << "Existing application removed successfully.";
    }

    QDir().mkpath(targetDir); // Ensure the Applications folder exists
    process.start("cp", {"-R", appPath, targetDir});
    process.waitForFinished();
    if (process.exitCode() != 0) {
        qWarning() << "Failed to copy application:" << process.readAllStandardError();
        return;
    }
    qDebug() << "Application copied to" << targetDir;

    // Unmount the .dmg
    process.start("hdiutil", {"detach", mountPoint});
    process.waitForFinished();
    if (process.exitCode() != 0) {
        qWarning() << "Failed to unmount .dmg:" << process.readAllStandardError();
        return;
    }
    qDebug() << ".dmg unmounted successfully.";

    // Run the application
    QString appExecutable = targetDir + "/Mudlet.app";
    process.setProcessEnvironment(env);
    process.start("open", {appExecutable});
    process.waitForFinished();
    if (process.exitCode() != 0) {
        qWarning() << "Failed to launch application:" << process.readAllStandardError();
        return;
    }
    qDebug() << "Application launched successfully.";
}


void installAndRunAppImage(QProcessEnvironment &env, const QString& tarFilePath) {
    QProcess process;

    // Extract the tar file
    QString extractDir = QDir::tempPath() + "/ExtractedApp"; // Temporary directory for extraction
    QDir().mkpath(extractDir); // Ensure the directory exists
    process.start("tar", {"-xf", tarFilePath, "-C", extractDir});
    process.waitForFinished();
    if (process.exitCode() != 0) {
        qWarning() << "Failed to extract tar file:" << process.readAllStandardError();
        return;
    }
    qDebug() << "Tar file extracted to" << extractDir;

    // Locate the AppImage file
    QDir dir(extractDir);
    QStringList appImages = dir.entryList({"*.AppImage"}, QDir::Files);
    if (appImages.isEmpty()) {
        qWarning() << "No AppImage file found in the extracted directory.";
        return;
    }
    QString appImagePath = dir.filePath(appImages.first());
    qDebug() << "Found AppImage:" << appImagePath;

    // Make the AppImage executable
    process.start("chmod", {"+x", appImagePath});
    process.waitForFinished();
    if (process.exitCode() != 0) {
        qWarning() << "Failed to make AppImage executable:" << process.readAllStandardError();
        return;
    }
    qDebug() << "AppImage is now executable.";

    // Run the AppImage
    process.setProcessEnvironment(env);
    process.start(appImagePath);
    process.waitForFinished();
    if (process.exitCode() != 0) {
        qWarning() << "Failed to run AppImage:" << process.readAllStandardError();
        return;
    }
    qDebug() << "AppImage launched successfully.";
}


void MudletBootstrap::installApplication(const QString &filePath) {

    QProcess installerProcess;

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

    // Read the profile from the .ini file
    QString launchProfile = readLaunchProfileFromResource();
    if (launchProfile.isEmpty()) {
        qDebug() << "No launch profile found. Using default.";
    } else {
        // Pass along the launch profile to the environment
        env.insert("MUDLET_PROFILES", launchProfile);
    }
    

    // Install the application
#if defined(Q_OS_WIN)
    installerProcess.setProcessEnvironment(env);
    installerProcess.start("cmd.exe", {"/C", outputFile});
    installerProcess.waitForFinished();
#elif defined(Q_OS_MAC)
    installAndRunDmg(env, outputFile);
#elif defined(Q_OS_LINUX)
    installAndRunAppImage(env, outputFile);
#endif
    
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


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

QString detectOS() {
    QString os;

#if defined(Q_OS_WIN)
    os = "Windows";
    // Check for 32-bit or 64-bit Windows
    if (sizeof(void*) == 8) { // Pointer size is 8 bytes for 64-bit
        os += " 64-bit";
    } else {
        os += " 32-bit";
    }

#elif defined(Q_OS_MAC)
    os = "macOS";
    // Check for Apple Silicon (ARM64) or Intel
    QString architecture = QSysInfo::currentCpuArchitecture();
    if (architecture.contains("arm64")) {
        os += " (Apple Silicon)";
    } else if (architecture.contains("x86_64")) {
        os += " (Intel)";
    } else {
        os += " (Unknown Architecture)";
    }

#elif defined(Q_OS_LINUX)
    os = "Linux";
    QString architecture = QSysInfo::currentCpuArchitecture();
    os += " (" + architecture + ")";
#else
    os = "Unknown OS";
#endif

    qDebug() << "Detected OS:" << os;
    return os;
}

QString readLaunchProfileFromResource() {
    QSettings settings(":/resources/launch.ini", QSettings::IniFormat);

    QString profile = settings.value("Settings/MUDLET_LAUNCH_PROFILE", "").toString();

    if (profile.isEmpty()) {
        qDebug() << "MUDLET_LAUNCH_PROFILE not found in resource file.";
    }

    return profile;
}

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
    QString os = detectOS();
    qDebug() << "Detected OS:" << os;

    QUrl url("https://www.mudlet.org/download/");
    QNetworkRequest request(url);

    currentReply = networkManager.get(request);
    connect(currentReply, &QNetworkReply::finished, this, &MudletBootstrap::onFetchHtmlFinished);

    // Show the progress bar window
    progressWindow->show();
}

void MudletBootstrap::onFetchHtmlFinished() {
    if (currentReply->error() != QNetworkReply::NoError) {
        qDebug() << "Error fetching HTML:" << currentReply->errorString();
        return;
    }

    fetchedHtml = currentReply->readAll();
    currentReply->deleteLater();

    QString os = detectOS();
    extractDownloadInfo(fetchedHtml, os);

    if (info.link.isEmpty() || info.appName.isEmpty() || info.sha256.isEmpty()) {
        qDebug() << "Failed to extract download information for OS:" << os;
        return;
    }

    qDebug() << "Download link:" << info.link;
    qDebug() << "Application name:" << info.appName;
    qDebug() << "SHA-256 checksum:" << info.sha256;

    outputFile = "downloaded_" + info.appName;

    // Create a request and start downloading
    QNetworkRequest request{QUrl(info.link)};
    currentReply = networkManager.get(request);

    connect(currentReply, &QNetworkReply::downloadProgress, this, &MudletBootstrap::onDownloadProgress);
    connect(currentReply, &QNetworkReply::finished, this, &MudletBootstrap::onDownloadFinished);
    connect(currentReply, &QNetworkReply::errorOccurred, this, &MudletBootstrap::onDownloadError);

    statusLabel->setText(QString("Downloading %1...").arg(info.appName));
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
        env.insert("MUDLET_LAUNCH_PROFILE", launchProfile);
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


void MudletBootstrap::extractDownloadInfo(const QString &html, const QString &os) {

    // For now, since the only Windows and Mac downloads are 32 bit (Windows) and x86 (macOS)
    // just grab those, this will need to be updated when additional downloads are available
    // for those platforms

    QRegularExpression regex;
    if (os.startsWith("Windows")) {
        regex.setPattern(R"(<strong><a href=\"([^\"]+)\">([^<]+ \(windows-32\))</a></strong>.*?sha256:\s*([a-fA-F0-9]{64}))");
    } else if (os.startsWith("macOS")) {
        regex.setPattern(R"(<strong><a href=\"([^\"]+)\">([^<]+ \(macOS\))</a></strong>.*?sha256:\s*([a-fA-F0-9]{64}))");
    } else if (os.startsWith("Linux")) {
        regex.setPattern(R"(<strong><a href=\"([^\"]+)\">([^<]+ \(Linux\))</a></strong>.*?sha256:\s*([a-fA-F0-9]{64}))");
    }

    QRegularExpressionMatch match = regex.match(html);
    if (match.hasMatch()) {
        info.link = match.captured(1);     // download link
        info.appName = match.captured(2);  // application name
        info.sha256 = match.captured(3);   // SHA-256
    }
}


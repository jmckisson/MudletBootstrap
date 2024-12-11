#ifndef MUDLETDOWNLOADER_H
#define MUDLETDOWNLOADER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QWidget>
#include <QProgressBar>
#include <QLabel>

struct DownloadInfo {
    QString url;
    QString appName;
    QString sha256;
};

class MudletBootstrap : public QObject {
    Q_OBJECT

public:
    explicit MudletBootstrap(QObject *parent = nullptr);
    void start();

private slots:
    void onFetchPlatformFeedFinished();
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onDownloadFinished();
    void onDownloadError(QNetworkReply::NetworkError error);

private:
    QNetworkAccessManager networkManager;
    QNetworkReply *currentReply;
    void fetchPlatformFeed();
    void downloadFile(const QString &url, const QString &outputFile);
    void installApplication(const QString &filePath);

    QWidget *progressWindow;
    QProgressBar *progressBar;
    QLabel *statusLabel;

    QString fetchedHtml;
    QString downloadLink;
    QString outputFile;

    DownloadInfo info;
};

#endif

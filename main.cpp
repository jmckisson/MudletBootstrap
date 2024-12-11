#include <QApplication>
#include <QProcess>
#include <QThread>
#include <QRegularExpression>
#include <QString>

#include "MudletBootstrap.h"

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);

    qDebug() << "Starting MudletDownloader...";

    MudletBootstrap app;
    app.start();

    return a.exec();
}

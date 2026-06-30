#include "gui/main_window.h"
#include <QApplication>
#include <QStringList>
#include <QFileInfo>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setStyle("Fusion");
    
    MainWindow mainWindow;

    // Parse command line arguments for MKF file path
    QStringList args = app.arguments();
    // args[0] is the executable path, check remaining args
    for (int i = 1; i < args.size(); ++i) {
        const QString& arg = args.at(i);
        QFileInfo fileInfo(arg);
        if (fileInfo.suffix().compare("mkf", Qt::CaseInsensitive) == 0 || fileInfo.exists()) {
            mainWindow.openFile(fileInfo.absoluteFilePath());
            break;
        }
    }

    mainWindow.show();
    
    return app.exec();
}
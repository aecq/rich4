#include "gui/main_window.h"
#include <QApplication>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setStyle("Fusion");
    
    MainWindow mainWindow;
    mainWindow.show();
    
    return app.exec();
}
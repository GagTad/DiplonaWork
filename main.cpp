#include "mainwindow.h"
#include <QApplication>
#include <QStyleFactory>
#include <QFont>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    // Set application metadata
    QCoreApplication::setOrganizationName("VLSI-EDA");
    QCoreApplication::setApplicationName("Standard Cell Placement Tool");
    QCoreApplication::setApplicationVersion("1.0.0");

    // Set modern style
    app.setStyle(QStyleFactory::create("Fusion"));

    // Set default font
    QFont font = app.font();
    font.setPointSize(10);
    app.setFont(font);

    // Create and show main window
    vlsi::gui::MainWindow mainWindow;
    mainWindow.show();

    return app.exec();
}

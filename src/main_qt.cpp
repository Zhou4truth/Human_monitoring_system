#include <QApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QDir>
#include <QDebug>
#include <QStyleFactory>
#include <QSplashScreen>
#include <QPixmap>
#include <QThread>
#include <iostream>

#include "ui/main_window.hpp"

int main(int argc, char *argv[])
{
    // Set application information
    QApplication::setApplicationName("Human Monitoring System");
    QApplication::setApplicationVersion("1.0.0");
    QApplication::setOrganizationName("HMS Organization");
    QApplication::setOrganizationDomain("hms.org");
    
    // Create Qt application
    QApplication app(argc, argv);
    
    // Set application style
    app.setStyle(QStyleFactory::create("Fusion"));
    
    // Create splash screen
    QSplashScreen splash(QPixmap(":/resources/splash.png"));
    splash.show();
    app.processEvents();
    
    // Parse command line arguments
    QCommandLineParser parser;
    parser.setApplicationDescription("Human Monitoring System - Fall Detection and Emergency Notification");
    parser.addHelpOption();
    parser.addVersionOption();
    
    QCommandLineOption configOption(QStringList() << "c" << "config", 
                                    "Specify configuration file", "config.json");
    parser.addOption(configOption);
    
    QCommandLineOption noGuiOption(QStringList() << "no-gui", 
                                  "Run in headless mode (no GUI)");
    parser.addOption(noGuiOption);
    
    parser.process(app);
    
    // Get configuration file
    QString configFile = parser.value(configOption);
    if (configFile.isEmpty()) {
        configFile = "config.json";
    }
    
    // Check if running in headless mode
    bool headless = parser.isSet(noGuiOption);
    
    // Create and show main window
    if (!headless) {
        // Create main window
        hms::MainWindow mainWindow;
        mainWindow.show();
        
        // Close splash screen
        splash.finish(&mainWindow);
        
        // Initialize application
        QThread::msleep(500); // Give UI time to render
        mainWindow.initialize();
        
        // Run application
        return app.exec();
    } else {
        // Headless mode
        std::cout << "Running in headless mode" << std::endl;
        
        // Create application without UI
        try {
            hms::Application app;
            if (!app.initialize(configFile.toStdString())) {
                std::cerr << "Failed to initialize application" << std::endl;
                return 1;
            }
            
            std::cout << "Application initialized successfully" << std::endl;
            
            // Run application
            app.run();
            
            // Wait for signal to exit
            std::cout << "Application is running. Press Ctrl+C to exit." << std::endl;
            
            // Main thread will wait for signal
            while (true) {
                QThread::sleep(1);
            }
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
            return 1;
        }
    }
    
    return 0;
}

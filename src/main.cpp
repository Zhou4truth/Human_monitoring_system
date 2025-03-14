#include "core/application.hpp"
#include <iostream>
#include <string>
#include <csignal>

hms::Application* g_app = nullptr;

void signalHandler(int signal) {
    std::cout << "Received signal " << signal << ", shutting down..." << std::endl;
    if (g_app) {
        g_app->stop();
    }
    exit(signal);
}

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --config <file>        Specify configuration file (default: config.json)" << std::endl;
    std::cout << "  --add-camera <uri>     Add camera with URI" << std::endl;
    std::cout << "  --camera-type <type>   Specify camera type (USB, RTSP, HTTP, MJPEG)" << std::endl;
    std::cout << "  --recording-dir <dir>  Specify recording directory (default: recordings)" << std::endl;
    std::cout << "  --no-fall-detection    Disable fall detection" << std::endl;
    std::cout << "  --no-privacy           Disable privacy protection" << std::endl;
    std::cout << "  --no-recording         Disable recording" << std::endl;
    std::cout << "  --help                 Show this help message" << std::endl;
}

int main(int argc, char** argv) {
    // Register signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    // Parse command line arguments
    std::string configFile = "config.json";
    std::string cameraUri;
    std::string cameraType = "RTSP";
    std::string recordingDir = "recordings";
    bool fallDetectionEnabled = true;
    bool privacyProtectionEnabled = true;
    bool recordingEnabled = true;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "--config" && i + 1 < argc) {
            configFile = argv[++i];
        } else if (arg == "--add-camera" && i + 1 < argc) {
            cameraUri = argv[++i];
        } else if (arg == "--camera-type" && i + 1 < argc) {
            cameraType = argv[++i];
        } else if (arg == "--recording-dir" && i + 1 < argc) {
            recordingDir = argv[++i];
        } else if (arg == "--no-fall-detection") {
            fallDetectionEnabled = false;
        } else if (arg == "--no-privacy") {
            privacyProtectionEnabled = false;
        } else if (arg == "--no-recording") {
            recordingEnabled = false;
        } else {
            std::cerr << "Unknown option: " << arg << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }
    
    try {
        // Create and initialize application
        hms::Application app;
        g_app = &app;
        
        std::cout << "Initializing Human Monitoring System..." << std::endl;
        
        if (!app.initialize(configFile)) {
            std::cerr << "Failed to initialize application" << std::endl;
            return 1;
        }
        
        // Apply command line settings
        app.enableFallDetection(fallDetectionEnabled);
        app.enablePrivacyProtection(privacyProtectionEnabled);
        app.enableRecording(recordingEnabled);
        app.setRecordingDirectory(recordingDir);
        
        // Add camera if specified
        if (!cameraUri.empty()) {
            hms::Camera::ConnectionType type;
            
            if (cameraType == "USB") {
                type = hms::Camera::ConnectionType::USB;
            } else if (cameraType == "RTSP") {
                type = hms::Camera::ConnectionType::RTSP;
            } else if (cameraType == "HTTP") {
                type = hms::Camera::ConnectionType::HTTP;
            } else if (cameraType == "MJPEG") {
                type = hms::Camera::ConnectionType::MJPEG;
            } else {
                std::cerr << "Unknown camera type: " << cameraType << std::endl;
                return 1;
            }
            
            std::cout << "Adding camera: " << cameraUri << " (Type: " << cameraType << ")" << std::endl;
            if (!app.addCamera(cameraUri, type)) {
                std::cerr << "Failed to add camera" << std::endl;
                return 1;
            }
        }
        
        // Run the application
        std::cout << "Starting Human Monitoring System..." << std::endl;
        app.run();
        
        // Wait for application to exit
        std::cout << "Human Monitoring System is running. Press Ctrl+C to exit." << std::endl;
        
        // Main thread will wait for signal
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}

// include/core/camera.hpp
#pragma once

#include <string>
#include <memory>
#include <vector>
#include <opencv2/opencv.hpp>

namespace hms {

// Forward declaration
std::string generateUniqueId();

class Camera {
public:
    enum class ConnectionType {
        USB,
        RTSP,
        HTTP,
        MJPEG
    };
    
    Camera(const std::string& uri, ConnectionType type);
    ~Camera();
    
    bool connect();
    bool disconnect();
    bool isConnected() const;
    
    cv::Mat getFrame();
    std::string getStatus() const;
    std::string getId() const;
    
private:
    std::string m_uri;
    ConnectionType m_type;
    cv::VideoCapture m_capture;
    bool m_connected;
    std::string m_id;
};

class CameraManager {
public:
    CameraManager();
    ~CameraManager();
    
    bool addCamera(const std::string& uri, Camera::ConnectionType type);
    bool removeCamera(const std::string& id);
    Camera* getCamera(size_t index);
    Camera* getCameraById(const std::string& id);
    std::vector<Camera*> getAllCameras();
    size_t getCameraCount() const;
    
private:
    std::vector<std::unique_ptr<Camera>> m_cameras;
};

} // namespace hms

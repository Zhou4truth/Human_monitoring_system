#include "core/camera.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <algorithm>
#include <uuid/uuid.h>

namespace hms {

// Generate a unique ID for the camera
std::string generateUniqueId() {
    uuid_t uuid;
    uuid_generate(uuid);
    char uuid_str[37];
    uuid_unparse_lower(uuid, uuid_str);
    return std::string(uuid_str);
}

Camera::Camera(const std::string& uri, ConnectionType type)
    : m_uri(uri), m_type(type), m_connected(false), m_id(generateUniqueId()) {
}

Camera::~Camera() {
    disconnect();
}

bool Camera::connect() {
    if (m_connected) {
        return true;
    }
    
    try {
        switch (m_type) {
            case ConnectionType::USB: {
                // Try to parse URI as a number for USB cameras
                int deviceId = 0;
                try {
                    deviceId = std::stoi(m_uri);
                } catch (const std::exception& e) {
                    std::cerr << "Failed to parse USB camera ID: " << e.what() << std::endl;
                    return false;
                }
                m_capture.open(deviceId);
                break;
            }
            case ConnectionType::RTSP:
            case ConnectionType::HTTP:
            case ConnectionType::MJPEG:
                m_capture.open(m_uri);
                break;
        }
        
        if (!m_capture.isOpened()) {
            std::cerr << "Failed to open camera: " << m_uri << std::endl;
            return false;
        }
        
        // Set camera properties
        m_capture.set(cv::CAP_PROP_FRAME_WIDTH, 1280);
        m_capture.set(cv::CAP_PROP_FRAME_HEIGHT, 720);
        m_capture.set(cv::CAP_PROP_FPS, 30);
        
        m_connected = true;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Exception while connecting to camera: " << e.what() << std::endl;
        return false;
    }
}

bool Camera::disconnect() {
    if (!m_connected) {
        return true;
    }
    
    try {
        m_capture.release();
        m_connected = false;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Exception while disconnecting camera: " << e.what() << std::endl;
        return false;
    }
}

bool Camera::isConnected() const {
    return m_connected && m_capture.isOpened();
}

cv::Mat Camera::getFrame() {
    cv::Mat frame;
    
    if (!m_connected && !connect()) {
        return frame;
    }
    
    try {
        if (!m_capture.read(frame)) {
            std::cerr << "Failed to read frame from camera: " << m_uri << std::endl;
            m_connected = false;
            
            // Try to reconnect
            disconnect();
            std::this_thread::sleep_for(std::chrono::seconds(1));
            connect();
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception while reading frame: " << e.what() << std::endl;
        m_connected = false;
    }
    
    return frame;
}

std::string Camera::getStatus() const {
    if (m_connected) {
        return "Connected";
    } else {
        return "Disconnected";
    }
}

std::string Camera::getId() const {
    return m_id;
}

// CameraManager implementation
CameraManager::CameraManager() {
}

CameraManager::~CameraManager() {
    for (auto& camera : m_cameras) {
        camera->disconnect();
    }
}

bool CameraManager::addCamera(const std::string& uri, Camera::ConnectionType type) {
    if (m_cameras.size() >= 4) {
        std::cerr << "Maximum number of cameras (4) already added." << std::endl;
        return false;
    }
    
    auto camera = std::make_unique<Camera>(uri, type);
    if (camera->connect()) {
        m_cameras.push_back(std::move(camera));
        return true;
    }
    return false;
}

bool CameraManager::removeCamera(const std::string& id) {
    auto it = std::find_if(m_cameras.begin(), m_cameras.end(),
                          [&id](const std::unique_ptr<Camera>& cam) {
                              return cam->getId() == id;
                          });
    
    if (it != m_cameras.end()) {
        (*it)->disconnect();
        m_cameras.erase(it);
        return true;
    }
    return false;
}

Camera* CameraManager::getCamera(size_t index) {
    if (index < m_cameras.size()) {
        return m_cameras[index].get();
    }
    return nullptr;
}

Camera* CameraManager::getCameraById(const std::string& id) {
    for (auto& camera : m_cameras) {
        if (camera->getId() == id) {
            return camera.get();
        }
    }
    return nullptr;
}

std::vector<Camera*> CameraManager::getAllCameras() {
    std::vector<Camera*> cameras;
    for (auto& camera : m_cameras) {
        cameras.push_back(camera.get());
    }
    return cameras;
}

size_t CameraManager::getCameraCount() const {
    return m_cameras.size();
}

} // namespace hms

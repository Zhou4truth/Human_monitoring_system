// include/core/camera.hpp
#pragma once

#include <string>
#include <memory>
#include <vector>
#include <opencv2/opencv.hpp>

namespace hms {

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
    std::unique_ptr<cv::VideoCapture> m_capture;
    bool m_connected;
    std::string m_id;
    std::string m_status;
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

// src/core/camera.cpp
#include "core/camera.hpp"
#include <iostream>
#include <chrono>
#include <thread>
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
    : m_uri(uri), m_type(type), m_connected(false), m_id(generateUniqueId()), m_status("Initialized") {
}

Camera::~Camera() {
    if (m_connected) {
        disconnect();
    }
}

bool Camera::connect() {
    try {
        m_capture = std::make_unique<cv::VideoCapture>();
        
        switch (m_type) {
            case ConnectionType::USB:
                // For USB cameras, the URI is typically a number (0, 1, etc.)
                m_capture->open(std::stoi(m_uri));
                break;
            case ConnectionType::RTSP:
            case ConnectionType::HTTP:
            case ConnectionType::MJPEG:
                // For network cameras, open the provided URI
                m_capture->open(m_uri);
                break;
        }
        
        if (!m_capture->isOpened()) {
            m_status = "Failed to connect";
            return false;
        }
        
        // Set camera properties
        m_capture->set(cv::CAP_PROP_FRAME_WIDTH, 1280);
        m_capture->set(cv::CAP_PROP_FRAME_HEIGHT, 720);
        m_capture->set(cv::CAP_PROP_FPS, 30);
        
        m_connected = true;
        m_status = "Connected";
        return true;
    } catch (const std::exception& e) {
        m_status = "Error: " + std::string(e.what());
        m_connected = false;
        return false;
    }
}

bool Camera::disconnect() {
    if (m_capture) {
        m_capture->release();
        m_capture.reset();
        m_connected = false;
        m_status = "Disconnected";
        return true;
    }
    return false;
}

bool Camera::isConnected() const {
    return m_connected && m_capture && m_capture->isOpened();
}

cv::Mat Camera::getFrame() {
    cv::Mat frame;
    if (m_connected && m_capture) {
        if (!m_capture->read(frame)) {
            m_status = "Frame read error";
            
            // Try to reconnect
            disconnect();
            std::this_thread::sleep_for(std::chrono::seconds(1));
            connect();
        }
    }
    return frame;
}

std::string Camera::getStatus() const {
    return m_status;
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

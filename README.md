# Human Monitoring System

A comprehensive system for monitoring humans, detecting falls, and sending emergency notifications to caregivers and healthcare providers.

## Overview

The Human Monitoring System (HMS) is designed to enhance the safety and well-being of elderly individuals and those with medical conditions by providing continuous monitoring, fall detection, and emergency response capabilities. The system uses computer vision and machine learning to detect human presence, identify falls, and protect privacy while ensuring timely assistance when needed.

## Features

- **Camera Management**: Support for multiple camera types (USB, RTSP, HTTP, MJPEG)
- **Human Detection**: Real-time detection and tracking of persons using YOLOv8
- **Fall Detection**: Advanced algorithms to identify falls and trigger alerts
- **Privacy Protection**: Automatic blurring of sensitive areas to maintain dignity
- **User Database**: Management of users, emergency contacts, and healthcare providers
- **Emergency Notifications**: SMS and email alerts to designated contacts
- **Video Recording**: Continuous or event-triggered recording with retention policies
- **User-friendly Interface**: Qt-based GUI for easy monitoring and management

## System Requirements

### Hardware
- CPU: Intel Core i5 or equivalent (i7 recommended for multiple cameras)
- RAM: 8GB minimum (16GB recommended)
- Storage: SSD with at least 100GB free space
- Camera: USB webcam or IP camera network

### Software
- Operating System: Ubuntu/Debian Linux
- C++ Compiler: GCC/G++ (C++17 or newer)
- Dependencies: OpenCV 4.7+, Qt 6.5+, SQLite, CURL, Boost, nlohmann/json

## Installation

### Prerequisites

1. Install required packages:
```bash
sudo apt update
sudo apt install build-essential cmake git libopencv-dev libboost-all-dev libcurl4-openssl-dev libsqlite3-dev qt6-base-dev
```

2. Clone the repository:
```bash
git clone https://github.com/yourusername/human_monitoring_system.git
cd human_monitoring_system
```

### Build

1. Create a build directory:
```bash
mkdir build
cd build
```

2. Configure with CMake:
```bash
cmake ..
```

3. Build the project:
```bash
make -j$(nproc)
```

4. Install (optional):
```bash
sudo make install
```

## Usage

### GUI Mode

Run the application with the graphical user interface:
```bash
./bin/HumanMonitoringSystem
```

### Headless Mode

Run the application without GUI (for servers or embedded systems):
```bash
./bin/HumanMonitoringSystem --no-gui
```

### Configuration

Edit the `config.json` file to customize settings:
- Camera configurations
- Detection parameters
- Recording settings
- Notification preferences

## Project Structure

```
human_monitoring_system/
├── include/                  # Header files
│   ├── core/                 # Core application components
│   ├── database/             # Database management
│   ├── detection/            # Human and fall detection
│   ├── network/              # Networking and notifications
│   └── ui/                   # User interface
├── src/                      # Source files
│   ├── core/                 # Core implementation
│   ├── database/             # Database implementation
│   ├── detection/            # Detection implementation
│   ├── network/              # Network implementation
│   └── ui/                   # UI implementation
├── resources/                # Application resources
├── scripts/                  # Utility scripts
├── models/                   # ML models (downloaded at build time)
├── tests/                    # Unit and integration tests
├── CMakeLists.txt            # CMake build configuration
├── config.json               # Application configuration
└── README.md                 # This file
```

## Development

### Adding a New Camera

1. Use the GUI to add a camera through the interface
2. Or edit the `config.json` file to add a new camera entry

### Creating a New User

1. Navigate to the User Management tab in the GUI
2. Click "Add User" and fill in the required information
3. Add emergency contacts for the user

### Customizing Fall Detection

Adjust the fall detection parameters in `config.json`:
```json
"fall_detection": {
    "enabled": true,
    "fall_threshold": 0.7,
    "consecutive_frames": 3,
    "cooldown_period": 60
}
```

## Security Considerations

- Store API keys and credentials securely
- Use strong passwords for email and SMS services
- Regularly update the software and dependencies
- Implement network security measures for IP cameras
- Follow privacy regulations regarding video surveillance

## Troubleshooting

### Common Issues

1. **Camera not detected**
   - Check camera connection
   - Verify camera URI is correct
   - Ensure camera drivers are installed

2. **Fall detection not working**
   - Adjust detection thresholds
   - Ensure camera positioning is optimal
   - Check lighting conditions

3. **Notifications not sending**
   - Verify API credentials
   - Check internet connection
   - Ensure contact information is correct

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Acknowledgments

- OpenCV for computer vision capabilities
- YOLOv8 for object detection
- Qt for the user interface framework
- All contributors and testers
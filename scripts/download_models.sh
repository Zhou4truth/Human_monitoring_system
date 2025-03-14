#!/bin/bash
# Script to download YOLOv8 models for human detection

# Create models directory if it doesn't exist
mkdir -p ../models

# Download YOLOv8n ONNX model
echo "Downloading YOLOv8n model..."
wget -O ../models/yolov8n.onnx https://github.com/ultralytics/assets/releases/download/v0.0.0/yolov8n.onnx

# Check if download was successful
if [ $? -eq 0 ]; then
    echo "YOLOv8n model downloaded successfully"
else
    echo "Failed to download YOLOv8n model"
    exit 1
fi

# Make the script executable
chmod +x ../models/yolov8n.onnx

echo "All models downloaded successfully"
exit 0

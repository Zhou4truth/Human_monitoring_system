// include/ui/main_window.hpp
#pragma once

#include <QMainWindow>
#include <QTimer>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QLineEdit>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QAction>
#include <QMenu>
#include <QMenuBar>
#include <QStatusBar>
#include <QMessageBox>
#include <QFileDialog>
#include <QCloseEvent>
#include <QImage>
#include <QPixmap>
#include <QDateTime>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QSettings>
#include <QSplitter>
#include <QScrollArea>
#include <QListWidget>
#include <QStandardItemModel>
#include <QTableView>
#include <QHeaderView>
#include <QDialog>
#include <QFormLayout>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QDialogButtonBox>
#include <QTextEdit>
#include <QSystemTrayIcon>

#include <opencv2/opencv.hpp>
#include "core/application.hpp"

namespace hms {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void initialize();
    bool isInitialized() const;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    // Camera management
    void onAddCameraClicked();
    void onRemoveCameraClicked();
    void onCameraSelected(int index);
    
    // User management
    void onAddUserClicked();
    void onEditUserClicked();
    void onDeleteUserClicked();
    void onUserSelected(int row, int column);
    
    // Settings
    void onFallDetectionToggled(bool checked);
    void onPrivacyProtectionToggled(bool checked);
    void onRecordingToggled(bool checked);
    void onRecordingDirClicked();
    
    // Alerts
    void onAlertReceived(int userId, int personId);
    void onAlertResponded(int userId, int personId, const std::string& response);
    
    // UI updates
    void updateCameraFeeds();
    void updateUserTable();
    void updateAlertTable();
    
    // Menu actions
    void onAboutClicked();
    void onExitClicked();
    void onSettingsClicked();

private:
    // UI components
    QWidget *m_centralWidget;
    QTabWidget *m_tabWidget;
    
    // Camera tab
    QWidget *m_cameraTab;
    QLabel *m_mainCameraView;
    QListWidget *m_cameraList;
    QPushButton *m_addCameraButton;
    QPushButton *m_removeCameraButton;
    QCheckBox *m_fallDetectionCheckbox;
    QCheckBox *m_privacyProtectionCheckbox;
    QCheckBox *m_recordingCheckbox;
    QPushButton *m_recordingDirButton;
    QLabel *m_recordingDirLabel;
    
    // User management tab
    QWidget *m_userTab;
    QTableWidget *m_userTable;
    QPushButton *m_addUserButton;
    QPushButton *m_editUserButton;
    QPushButton *m_deleteUserButton;
    
    // Alerts tab
    QWidget *m_alertTab;
    QTableWidget *m_alertTable;
    
    // Status bar
    QLabel *m_statusLabel;
    
    // Dialogs
    QDialog *m_addCameraDialog;
    QDialog *m_addUserDialog;
    QDialog *m_settingsDialog;
    
    // Application
    Application *m_app;
    QTimer *m_updateTimer;
    
    // Current state
    int m_selectedUserId;
    
    // Helper methods
    void createMenus();
    void createCameraTab();
    void createUserTab();
    void createAlertTab();
    void createAddCameraDialog();
    void createAddUserDialog();
    void createSettingsDialog();
    
    void updateCameraView(const cv::Mat& frame);
    void showAlert(const QString& title, const QString& message);
    
    // Convert between OpenCV and Qt image formats
    QImage matToQImage(const cv::Mat& mat);
};

} // namespace hms

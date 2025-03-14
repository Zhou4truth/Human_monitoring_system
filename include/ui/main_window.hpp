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
    ~MainWindow() override;
    
    void initialize();
    bool isInitialized() const;
    
protected:
    void closeEvent(QCloseEvent *event) override;
    
private slots:
    // Camera management slots
    void onAddCameraClicked();
    void onRemoveCameraClicked();
    void onCameraSelected(int index);
    
    // User management slots
    void onAddUserClicked();
    void onEditUserClicked();
    void onDeleteUserClicked();
    void onUserSelected(int row, int column);
    
    // Alert management
    void onAlertReceived(int userId, int personId);
    void onAlertResponded(int userId, int personId, const std::string& response);
    
    // Settings slots
    void onFallDetectionToggled(bool checked);
    void onPrivacyProtectionToggled(bool checked);
    void onRecordingToggled(bool checked);
    void onRecordingDirClicked();
    
    // Menu action slots
    void onAboutClicked();
    void onExitClicked();
    void onSettingsClicked();
    
private:
    Application* m_app;
    QTimer* m_updateTimer;
    
    // UI components
    QWidget* m_centralWidget;
    QTabWidget* m_tabWidget;
    QLabel* m_statusLabel;
    
    // Camera tab
    QWidget* m_cameraTab;
    QComboBox* m_cameraSelector;
    QLabel* m_cameraView;
    QPushButton* m_addCameraBtn;
    QPushButton* m_removeCameraBtn;
    QCheckBox* m_fallDetectionChk;
    QCheckBox* m_privacyProtectionChk;
    QCheckBox* m_recordingChk;
    
    // User tab
    QWidget* m_userTab;
    QTableWidget* m_userTable;
    QPushButton* m_addUserBtn;
    QPushButton* m_editUserBtn;
    QPushButton* m_deleteUserBtn;
    
    // Alert tab
    QWidget* m_alertTab;
    QTableWidget* m_alertTable;
    
    // Dialogs
    QDialog* m_addCameraDialog;
    QLineEdit* m_cameraNameEdit;
    QLineEdit* m_cameraUrlEdit;
    QComboBox* m_cameraTypeCombo;
    
    QDialog* m_addUserDialog;
    QLineEdit* m_userNameEdit;
    QLineEdit* m_userEmailEdit;
    QLineEdit* m_userPhoneEdit;
    QLineEdit* m_userAddressEdit;
    
    QDialog* m_settingsDialog;
    QLineEdit* m_recordingDirEdit;
    QPushButton* m_recordingDirBtn;
    QSpinBox* m_retentionDaysSpinBox;
    
    // State
    int m_selectedUserId;
    
    // Helper methods
    void updateCameraView(const cv::Mat& frame);
    void updateUserTable();
    void updateAlertTable();
    void updateCameraFeeds();
    
    // UI creation methods
    void createMenus();
    void createCameraTab();
    void createUserTab();
    void createAlertTab();
    void createAddCameraDialog();
    void createAddUserDialog();
    void createSettingsDialog();
    
    // Utility methods
    QImage matToQImage(const cv::Mat& mat);
    void showAlert(const QString& title, const QString& message);
};

} // namespace hms

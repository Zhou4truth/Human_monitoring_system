// src/ui/main_window.cpp
#include "ui/main_window.hpp"
#include <QMessageBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QStandardItemModel>
#include <QHeaderView>
#include <QDateTime>
#include <QSettings>
#include <QDesktopServices>
#include <QUrl>

namespace hms {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_app(nullptr)
    , m_updateTimer(new QTimer(this))
    , m_selectedUserId(-1)
{
    setWindowTitle("Human Monitoring System");
    setMinimumSize(1024, 768);
    
    // Initialize UI components
    m_centralWidget = new QWidget(this);
    setCentralWidget(m_centralWidget);
    
    m_tabWidget = new QTabWidget(m_centralWidget);
    
    // Create tabs
    createCameraTab();
    createUserTab();
    createAlertTab();
    
    // Create layout
    QVBoxLayout *mainLayout = new QVBoxLayout(m_centralWidget);
    mainLayout->addWidget(m_tabWidget);
    m_centralWidget->setLayout(mainLayout);
    
    // Create status bar
    m_statusLabel = new QLabel("Ready");
    statusBar()->addWidget(m_statusLabel);
    
    // Create menus
    createMenus();
    
    // Create dialogs
    createAddCameraDialog();
    createAddUserDialog();
    createSettingsDialog();
    
    // Connect timer for updating camera feeds
    connect(m_updateTimer, &QTimer::timeout, this, &MainWindow::updateCameraFeeds);
    m_updateTimer->start(33); // ~30 fps
}

MainWindow::~MainWindow()
{
    if (m_app) {
        m_app->stop();
        delete m_app;
    }
}

void MainWindow::initialize()
{
    try {
        m_app = new Application();
        if (!m_app->initialize("config.json")) {
            QMessageBox::critical(this, "Error", "Failed to initialize application");
            return;
        }
        
        // Update UI with current settings
        m_fallDetectionCheckbox->setChecked(m_app->isFallDetectionEnabled());
        m_privacyProtectionCheckbox->setChecked(m_app->isPrivacyProtectionEnabled());
        m_recordingCheckbox->setChecked(m_app->isRecordingEnabled());
        m_recordingDirLabel->setText(QString::fromStdString(m_app->getRecordingDirectory()));
        
        // Update camera list
        for (size_t i = 0; i < m_app->getCameraCount(); ++i) {
            const auto& camera = m_app->getCameraInfo(i);
            m_cameraList->addItem(QString::fromStdString(camera.name));
        }
        
        // Update user table
        updateUserTable();
        
        // Start application
        m_app->run();
        
        m_statusLabel->setText("Application initialized successfully");
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Error", QString("Failed to initialize application: %1").arg(e.what()));
    }
}

bool MainWindow::isInitialized() const
{
    return m_app != nullptr && m_app->isRunning();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "Exit", "Are you sure you want to exit?",
        QMessageBox::Yes | QMessageBox::No
    );
    
    if (reply == QMessageBox::Yes) {
        if (m_app) {
            m_app->stop();
        }
        event->accept();
    } else {
        event->ignore();
    }
}

// Camera management slots
void MainWindow::onAddCameraClicked()
{
    if (m_addCameraDialog->exec() == QDialog::Accepted) {
        QLineEdit *uriEdit = m_addCameraDialog->findChild<QLineEdit*>("uriEdit");
        QComboBox *typeCombo = m_addCameraDialog->findChild<QComboBox*>("typeCombo");
        QLineEdit *nameEdit = m_addCameraDialog->findChild<QLineEdit*>("nameEdit");
        
        if (!uriEdit || !typeCombo || !nameEdit) {
            QMessageBox::warning(this, "Error", "Dialog components not found");
            return;
        }
        
        std::string uri = uriEdit->text().toStdString();
        std::string name = nameEdit->text().toStdString();
        
        if (uri.empty()) {
            QMessageBox::warning(this, "Error", "Camera URI cannot be empty");
            return;
        }
        
        Camera::ConnectionType type;
        switch (typeCombo->currentIndex()) {
            case 0: type = Camera::ConnectionType::USB; break;
            case 1: type = Camera::ConnectionType::RTSP; break;
            case 2: type = Camera::ConnectionType::HTTP; break;
            case 3: type = Camera::ConnectionType::MJPEG; break;
            default: type = Camera::ConnectionType::RTSP;
        }
        
        if (m_app && m_app->addCamera(uri, type, name)) {
            m_cameraList->addItem(QString::fromStdString(name));
            m_statusLabel->setText("Camera added successfully");
        } else {
            QMessageBox::warning(this, "Error", "Failed to add camera");
        }
    }
}

void MainWindow::onRemoveCameraClicked()
{
    int index = m_cameraList->currentRow();
    if (index >= 0) {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this, "Remove Camera", "Are you sure you want to remove this camera?",
            QMessageBox::Yes | QMessageBox::No
        );
        
        if (reply == QMessageBox::Yes && m_app) {
            if (m_app->removeCamera(index)) {
                m_cameraList->takeItem(index);
                m_statusLabel->setText("Camera removed successfully");
            } else {
                QMessageBox::warning(this, "Error", "Failed to remove camera");
            }
        }
    } else {
        QMessageBox::information(this, "Information", "Please select a camera to remove");
    }
}

void MainWindow::onCameraSelected(int index)
{
    if (index >= 0 && m_app) {
        // Update the main camera view with the selected camera
        m_statusLabel->setText(QString("Selected camera: %1").arg(m_cameraList->item(index)->text()));
    }
}

// User management slots
void MainWindow::onAddUserClicked()
{
    if (m_addUserDialog->exec() == QDialog::Accepted) {
        QLineEdit *nameEdit = m_addUserDialog->findChild<QLineEdit*>("nameEdit");
        QLineEdit *ageEdit = m_addUserDialog->findChild<QLineEdit*>("ageEdit");
        QLineEdit *addressEdit = m_addUserDialog->findChild<QLineEdit*>("addressEdit");
        QLineEdit *phoneEdit = m_addUserDialog->findChild<QLineEdit*>("phoneEdit");
        QLineEdit *emailEdit = m_addUserDialog->findChild<QLineEdit*>("emailEdit");
        
        if (!nameEdit || !ageEdit || !addressEdit || !phoneEdit || !emailEdit) {
            QMessageBox::warning(this, "Error", "Dialog components not found");
            return;
        }
        
        std::string name = nameEdit->text().toStdString();
        int age = ageEdit->text().toInt();
        std::string address = addressEdit->text().toStdString();
        std::string phone = phoneEdit->text().toStdString();
        std::string email = emailEdit->text().toStdString();
        
        if (name.empty()) {
            QMessageBox::warning(this, "Error", "User name cannot be empty");
            return;
        }
        
        User user;
        user.name = name;
        user.age = age;
        user.address = address;
        user.phoneNumber = phone;
        user.email = email;
        
        if (m_app && m_app->getUserDatabase().addUser(user)) {
            updateUserTable();
            m_statusLabel->setText("User added successfully");
        } else {
            QMessageBox::warning(this, "Error", "Failed to add user");
        }
    }
}

void MainWindow::onEditUserClicked()
{
    if (m_selectedUserId < 0) {
        QMessageBox::information(this, "Information", "Please select a user to edit");
        return;
    }
    
    if (!m_app) return;
    
    User user = m_app->getUserDatabase().getUserById(m_selectedUserId);
    if (user.id < 0) {
        QMessageBox::warning(this, "Error", "Failed to retrieve user information");
        return;
    }
    
    // Pre-fill dialog with user information
    QLineEdit *nameEdit = m_addUserDialog->findChild<QLineEdit*>("nameEdit");
    QLineEdit *ageEdit = m_addUserDialog->findChild<QLineEdit*>("ageEdit");
    QLineEdit *addressEdit = m_addUserDialog->findChild<QLineEdit*>("addressEdit");
    QLineEdit *phoneEdit = m_addUserDialog->findChild<QLineEdit*>("phoneEdit");
    QLineEdit *emailEdit = m_addUserDialog->findChild<QLineEdit*>("emailEdit");
    
    if (!nameEdit || !ageEdit || !addressEdit || !phoneEdit || !emailEdit) {
        QMessageBox::warning(this, "Error", "Dialog components not found");
        return;
    }
    
    nameEdit->setText(QString::fromStdString(user.name));
    ageEdit->setText(QString::number(user.age));
    addressEdit->setText(QString::fromStdString(user.address));
    phoneEdit->setText(QString::fromStdString(user.phoneNumber));
    emailEdit->setText(QString::fromStdString(user.email));
    
    if (m_addUserDialog->exec() == QDialog::Accepted) {
        user.name = nameEdit->text().toStdString();
        user.age = ageEdit->text().toInt();
        user.address = addressEdit->text().toStdString();
        user.phoneNumber = phoneEdit->text().toStdString();
        user.email = emailEdit->text().toStdString();
        
        if (m_app->getUserDatabase().updateUser(user)) {
            updateUserTable();
            m_statusLabel->setText("User updated successfully");
        } else {
            QMessageBox::warning(this, "Error", "Failed to update user");
        }
    }
}

void MainWindow::onDeleteUserClicked()
{
    if (m_selectedUserId < 0) {
        QMessageBox::information(this, "Information", "Please select a user to delete");
        return;
    }
    
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "Delete User", "Are you sure you want to delete this user?",
        QMessageBox::Yes | QMessageBox::No
    );
    
    if (reply == QMessageBox::Yes && m_app) {
        if (m_app->getUserDatabase().deleteUser(m_selectedUserId)) {
            updateUserTable();
            m_selectedUserId = -1;
            m_statusLabel->setText("User deleted successfully");
        } else {
            QMessageBox::warning(this, "Error", "Failed to delete user");
        }
    }
}

void MainWindow::onUserSelected(int row, int column)
{
    if (row >= 0 && m_app) {
        bool ok;
        m_selectedUserId = m_userTable->item(row, 0)->text().toInt(&ok);
        if (!ok) {
            m_selectedUserId = -1;
        }
    }
}

// Alert management
void MainWindow::onAlertReceived(int userId, int personId)
{
    if (!m_app) return;
    
    // Get user information
    User user = m_app->getUserDatabase().getUserById(userId);
    if (user.id < 0) {
        QMessageBox::warning(this, "Error", "Failed to retrieve user information for alert");
        return;
    }
    
    // Show alert dialog
    QString message = QString("Fall detected for user %1 (ID: %2)!\nEmergency contacts are being notified.")
                        .arg(QString::fromStdString(user.name))
                        .arg(userId);
    
    QMessageBox alertBox(QMessageBox::Warning, "Fall Alert", message, 
                         QMessageBox::Ok, this);
    alertBox.setModal(false);
    alertBox.show();
    
    // Update alert table
    updateAlertTable();
    
    // Switch to alert tab
    m_tabWidget->setCurrentWidget(m_alertTab);
    
    // Play alert sound
    QApplication::beep();
}

void MainWindow::onAlertResponded(int userId, int personId, const std::string& response)
{
    if (!m_app) return;
    
    // Get user information
    User user = m_app->getUserDatabase().getUserById(userId);
    if (user.id < 0) {
        return;
    }
    
    // Show response dialog
    QString message = QString("Response received for fall alert of user %1 (ID: %2):\n%3")
                        .arg(QString::fromStdString(user.name))
                        .arg(userId)
                        .arg(QString::fromStdString(response));
    
    QMessageBox::information(this, "Alert Response", message);
    
    // Update alert table
    updateAlertTable();
}

void MainWindow::updateAlertTable()
{
    if (!m_app) return;
    
    m_alertTable->clear();
    m_alertTable->setRowCount(0);
    m_alertTable->setColumnCount(5);
    
    m_alertTable->setHorizontalHeaderItem(0, new QTableWidgetItem("ID"));
    m_alertTable->setHorizontalHeaderItem(1, new QTableWidgetItem("User"));
    m_alertTable->setHorizontalHeaderItem(2, new QTableWidgetItem("Time"));
    m_alertTable->setHorizontalHeaderItem(3, new QTableWidgetItem("Status"));
    m_alertTable->setHorizontalHeaderItem(4, new QTableWidgetItem("Response"));
    
    // In a real implementation, we would get the alerts from the application
    // For now, we'll just show a placeholder
    // This would be replaced with actual alert data
    /*
    for (const auto& alert : m_app->getAlerts()) {
        int row = m_alertTable->rowCount();
        m_alertTable->insertRow(row);
        
        User user = m_app->getUserDatabase().getUserById(alert.userId);
        
        QTableWidgetItem *idItem = new QTableWidgetItem(QString::number(alert.id));
        QTableWidgetItem *userItem = new QTableWidgetItem(QString::fromStdString(user.name));
        QTableWidgetItem *timeItem = new QTableWidgetItem(QDateTime::fromTime_t(alert.timestamp).toString());
        QTableWidgetItem *statusItem = new QTableWidgetItem(alert.responded ? "Responded" : "Pending");
        QTableWidgetItem *responseItem = new QTableWidgetItem(QString::fromStdString(alert.response));
        
        m_alertTable->setItem(row, 0, idItem);
        m_alertTable->setItem(row, 1, userItem);
        m_alertTable->setItem(row, 2, timeItem);
        m_alertTable->setItem(row, 3, statusItem);
        m_alertTable->setItem(row, 4, responseItem);
    }
    */
}

void MainWindow::updateCameraFeeds()
{
    if (!m_app) return;
    
    int selectedCamera = m_cameraList->currentRow();
    if (selectedCamera >= 0 && selectedCamera < static_cast<int>(m_app->getCameraCount())) {
        cv::Mat frame = m_app->getProcessedFrame(selectedCamera);
        if (!frame.empty()) {
            updateCameraView(frame);
        }
    }
}

// Settings slots
void MainWindow::onFallDetectionToggled(bool checked)
{
    if (m_app) {
        m_app->enableFallDetection(checked);
        m_statusLabel->setText(QString("Fall detection %1").arg(checked ? "enabled" : "disabled"));
    }
}

void MainWindow::onPrivacyProtectionToggled(bool checked)
{
    if (m_app) {
        m_app->enablePrivacyProtection(checked);
        m_statusLabel->setText(QString("Privacy protection %1").arg(checked ? "enabled" : "disabled"));
    }
}

void MainWindow::onRecordingToggled(bool checked)
{
    if (m_app) {
        m_app->enableRecording(checked);
        m_statusLabel->setText(QString("Recording %1").arg(checked ? "enabled" : "disabled"));
    }
}

void MainWindow::onRecordingDirClicked()
{
    QString dir = QFileDialog::getExistingDirectory(
        this, "Select Recording Directory",
        QString::fromStdString(m_app ? m_app->getRecordingDirectory() : ""),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );
    
    if (!dir.isEmpty() && m_app) {
        m_app->setRecordingDirectory(dir.toStdString());
        m_recordingDirLabel->setText(dir);
        m_statusLabel->setText("Recording directory updated");
    }
}

void MainWindow::updateUserTable()
{
    if (!m_app) return;
    
    m_userTable->clear();
    m_userTable->setRowCount(0);
    m_userTable->setColumnCount(5);
    
    m_userTable->setHorizontalHeaderItem(0, new QTableWidgetItem("ID"));
    m_userTable->setHorizontalHeaderItem(1, new QTableWidgetItem("Name"));
    m_userTable->setHorizontalHeaderItem(2, new QTableWidgetItem("Age"));
    m_userTable->setHorizontalHeaderItem(3, new QTableWidgetItem("Address"));
    m_userTable->setHorizontalHeaderItem(4, new QTableWidgetItem("Phone Number"));
    
    for (const auto& user : m_app->getUserDatabase().getUsers()) {
        int row = m_userTable->rowCount();
        m_userTable->insertRow(row);
        
        QTableWidgetItem *idItem = new QTableWidgetItem(QString::number(user.id));
        QTableWidgetItem *nameItem = new QTableWidgetItem(QString::fromStdString(user.name));
        QTableWidgetItem *ageItem = new QTableWidgetItem(QString::number(user.age));
        QTableWidgetItem *addressItem = new QTableWidgetItem(QString::fromStdString(user.address));
        QTableWidgetItem *phoneItem = new QTableWidgetItem(QString::fromStdString(user.phoneNumber));
        
        m_userTable->setItem(row, 0, idItem);
        m_userTable->setItem(row, 1, nameItem);
        m_userTable->setItem(row, 2, ageItem);
        m_userTable->setItem(row, 3, addressItem);
        m_userTable->setItem(row, 4, phoneItem);
    }
}

// UI creation methods
void MainWindow::createMenus()
{
    QMenu *fileMenu = menuBar()->addMenu("&File");
    QMenu *viewMenu = menuBar()->addMenu("&View");
    QMenu *helpMenu = menuBar()->addMenu("&Help");
    
    // File menu
    QAction *settingsAction = new QAction("&Settings", this);
    connect(settingsAction, &QAction::triggered, this, &MainWindow::onSettingsClicked);
    fileMenu->addAction(settingsAction);
    
    QAction *exitAction = new QAction("E&xit", this);
    connect(exitAction, &QAction::triggered, this, &MainWindow::onExitClicked);
    fileMenu->addAction(exitAction);
    
    // View menu
    QAction *refreshAction = new QAction("&Refresh", this);
    connect(refreshAction, &QAction::triggered, [this]() {
        updateCameraFeeds();
        updateUserTable();
        updateAlertTable();
    });
    viewMenu->addAction(refreshAction);
    
    // Help menu
    QAction *aboutAction = new QAction("&About", this);
    connect(aboutAction, &QAction::triggered, this, &MainWindow::onAboutClicked);
    helpMenu->addAction(aboutAction);
}

void MainWindow::createCameraTab()
{
    m_cameraTab = new QWidget();
    m_tabWidget->addTab(m_cameraTab, "Camera Feeds");
    
    // Main camera view
    m_mainCameraView = new QLabel();
    m_mainCameraView->setMinimumSize(640, 480);
    m_mainCameraView->setAlignment(Qt::AlignCenter);
    m_mainCameraView->setText("No camera selected");
    m_mainCameraView->setStyleSheet("border: 1px solid #ccc;");
    
    // Camera list
    m_cameraList = new QListWidget();
    connect(m_cameraList, &QListWidget::currentRowChanged, this, &MainWindow::onCameraSelected);
    
    // Camera controls
    m_addCameraButton = new QPushButton("Add Camera");
    connect(m_addCameraButton, &QPushButton::clicked, this, &MainWindow::onAddCameraClicked);
    
    m_removeCameraButton = new QPushButton("Remove Camera");
    connect(m_removeCameraButton, &QPushButton::clicked, this, &MainWindow::onRemoveCameraClicked);
    
    QVBoxLayout *cameraControlsLayout = new QVBoxLayout();
    cameraControlsLayout->addWidget(m_cameraList);
    cameraControlsLayout->addWidget(m_addCameraButton);
    cameraControlsLayout->addWidget(m_removeCameraButton);
    
    // Settings
    QGroupBox *settingsGroup = new QGroupBox("Settings");
    
    m_fallDetectionCheckbox = new QCheckBox("Enable Fall Detection");
    connect(m_fallDetectionCheckbox, &QCheckBox::toggled, this, &MainWindow::onFallDetectionToggled);
    
    m_privacyProtectionCheckbox = new QCheckBox("Enable Privacy Protection");
    connect(m_privacyProtectionCheckbox, &QCheckBox::toggled, this, &MainWindow::onPrivacyProtectionToggled);
    
    m_recordingCheckbox = new QCheckBox("Enable Recording");
    connect(m_recordingCheckbox, &QCheckBox::toggled, this, &MainWindow::onRecordingToggled);
    
    m_recordingDirButton = new QPushButton("Recording Directory");
    connect(m_recordingDirButton, &QPushButton::clicked, this, &MainWindow::onRecordingDirClicked);
    
    m_recordingDirLabel = new QLabel("recordings");
    m_recordingDirLabel->setWordWrap(true);
    
    QVBoxLayout *settingsLayout = new QVBoxLayout();
    settingsLayout->addWidget(m_fallDetectionCheckbox);
    settingsLayout->addWidget(m_privacyProtectionCheckbox);
    settingsLayout->addWidget(m_recordingCheckbox);
    settingsLayout->addWidget(m_recordingDirButton);
    settingsLayout->addWidget(m_recordingDirLabel);
    settingsGroup->setLayout(settingsLayout);
    
    // Layout
    QVBoxLayout *rightLayout = new QVBoxLayout();
    rightLayout->addLayout(cameraControlsLayout);
    rightLayout->addWidget(settingsGroup);
    rightLayout->addStretch();
    
    QHBoxLayout *mainLayout = new QHBoxLayout(m_cameraTab);
    mainLayout->addWidget(m_mainCameraView, 3);
    mainLayout->addLayout(rightLayout, 1);
    m_cameraTab->setLayout(mainLayout);
}

void MainWindow::createUserTab()
{
    m_userTab = new QWidget();
    m_tabWidget->addTab(m_userTab, "User Management");
    
    // User table
    m_userTable = new QTableWidget();
    m_userTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_userTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_userTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(m_userTable, &QTableWidget::cellClicked, this, &MainWindow::onUserSelected);
    
    // User controls
    m_addUserButton = new QPushButton("Add User");
    connect(m_addUserButton, &QPushButton::clicked, this, &MainWindow::onAddUserClicked);
    
    m_editUserButton = new QPushButton("Edit User");
    connect(m_editUserButton, &QPushButton::clicked, this, &MainWindow::onEditUserClicked);
    
    m_deleteUserButton = new QPushButton("Delete User");
    connect(m_deleteUserButton, &QPushButton::clicked, this, &MainWindow::onDeleteUserClicked);
    
    QHBoxLayout *userControlsLayout = new QHBoxLayout();
    userControlsLayout->addWidget(m_addUserButton);
    userControlsLayout->addWidget(m_editUserButton);
    userControlsLayout->addWidget(m_deleteUserButton);
    userControlsLayout->addStretch();
    
    QVBoxLayout *mainLayout = new QVBoxLayout(m_userTab);
    mainLayout->addWidget(m_userTable);
    mainLayout->addLayout(userControlsLayout);
    m_userTab->setLayout(mainLayout);
}

void MainWindow::createAlertTab()
{
    m_alertTab = new QWidget();
    m_tabWidget->addTab(m_alertTab, "Alerts");
    
    // Alert table
    m_alertTable = new QTableWidget();
    m_alertTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_alertTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_alertTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    
    QVBoxLayout *mainLayout = new QVBoxLayout(m_alertTab);
    mainLayout->addWidget(m_alertTable);
    m_alertTab->setLayout(mainLayout);
}

void MainWindow::createAddCameraDialog()
{
    m_addCameraDialog = new QDialog(this);
    m_addCameraDialog->setWindowTitle("Add Camera");
    m_addCameraDialog->setModal(true);
    
    QLineEdit *uriEdit = new QLineEdit();
    uriEdit->setObjectName("uriEdit");
    uriEdit->setPlaceholderText("Camera URI (e.g., 0 for webcam, rtsp:// for IP camera)");
    
    QComboBox *typeCombo = new QComboBox();
    typeCombo->setObjectName("typeCombo");
    typeCombo->addItem("USB");
    typeCombo->addItem("RTSP");
    typeCombo->addItem("HTTP");
    typeCombo->addItem("MJPEG");
    
    QLineEdit *nameEdit = new QLineEdit();
    nameEdit->setObjectName("nameEdit");
    nameEdit->setPlaceholderText("Camera Name (e.g., Living Room)");
    
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, m_addCameraDialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, m_addCameraDialog, &QDialog::reject);
    
    QFormLayout *formLayout = new QFormLayout();
    formLayout->addRow("Camera URI:", uriEdit);
    formLayout->addRow("Camera Type:", typeCombo);
    formLayout->addRow("Camera Name:", nameEdit);
    
    QVBoxLayout *mainLayout = new QVBoxLayout();
    mainLayout->addLayout(formLayout);
    mainLayout->addWidget(buttonBox);
    
    m_addCameraDialog->setLayout(mainLayout);
}

void MainWindow::createAddUserDialog()
{
    m_addUserDialog = new QDialog(this);
    m_addUserDialog->setWindowTitle("Add/Edit User");
    m_addUserDialog->setModal(true);
    
    QLineEdit *nameEdit = new QLineEdit();
    nameEdit->setObjectName("nameEdit");
    nameEdit->setPlaceholderText("User Name");
    
    QLineEdit *ageEdit = new QLineEdit();
    ageEdit->setObjectName("ageEdit");
    ageEdit->setPlaceholderText("Age");
    ageEdit->setValidator(new QIntValidator(0, 150, this));
    
    QLineEdit *addressEdit = new QLineEdit();
    addressEdit->setObjectName("addressEdit");
    addressEdit->setPlaceholderText("Address");
    
    QLineEdit *phoneEdit = new QLineEdit();
    phoneEdit->setObjectName("phoneEdit");
    phoneEdit->setPlaceholderText("Phone Number");
    
    QLineEdit *emailEdit = new QLineEdit();
    emailEdit->setObjectName("emailEdit");
    emailEdit->setPlaceholderText("Email");
    
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, m_addUserDialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, m_addUserDialog, &QDialog::reject);
    
    QFormLayout *formLayout = new QFormLayout();
    formLayout->addRow("Name:", nameEdit);
    formLayout->addRow("Age:", ageEdit);
    formLayout->addRow("Address:", addressEdit);
    formLayout->addRow("Phone Number:", phoneEdit);
    formLayout->addRow("Email:", emailEdit);
    
    QVBoxLayout *mainLayout = new QVBoxLayout();
    mainLayout->addLayout(formLayout);
    mainLayout->addWidget(buttonBox);
    
    m_addUserDialog->setLayout(mainLayout);
}

void MainWindow::createSettingsDialog()
{
    m_settingsDialog = new QDialog(this);
    m_settingsDialog->setWindowTitle("Settings");
    m_settingsDialog->setModal(true);
    
    QTabWidget *tabWidget = new QTabWidget();
    
    // General settings tab
    QWidget *generalTab = new QWidget();
    QFormLayout *generalLayout = new QFormLayout(generalTab);
    
    QSpinBox *frameRateSpin = new QSpinBox();
    frameRateSpin->setRange(1, 60);
    frameRateSpin->setValue(30);
    generalLayout->addRow("Frame Rate:", frameRateSpin);
    
    QComboBox *resolutionCombo = new QComboBox();
    resolutionCombo->addItem("640x480");
    resolutionCombo->addItem("1280x720");
    resolutionCombo->addItem("1920x1080");
    generalLayout->addRow("Resolution:", resolutionCombo);
    
    tabWidget->addTab(generalTab, "General");
    
    // Detection settings tab
    QWidget *detectionTab = new QWidget();
    QFormLayout *detectionLayout = new QFormLayout(detectionTab);
    
    QDoubleSpinBox *confidenceSpin = new QDoubleSpinBox();
    confidenceSpin->setRange(0.1, 1.0);
    confidenceSpin->setValue(0.5);
    confidenceSpin->setSingleStep(0.05);
    detectionLayout->addRow("Detection Confidence:", confidenceSpin);
    
    QSpinBox *fallThresholdSpin = new QSpinBox();
    fallThresholdSpin->setRange(1, 10);
    fallThresholdSpin->setValue(3);
    detectionLayout->addRow("Fall Detection Threshold:", fallThresholdSpin);
    
    tabWidget->addTab(detectionTab, "Detection");
    
    // Notification settings tab
    QWidget *notificationTab = new QWidget();
    QFormLayout *notificationLayout = new QFormLayout(notificationTab);
    
    QCheckBox *smsCheckbox = new QCheckBox("Enable SMS Notifications");
    smsCheckbox->setChecked(true);
    notificationLayout->addRow(smsCheckbox);
    
    QCheckBox *emailCheckbox = new QCheckBox("Enable Email Notifications");
    emailCheckbox->setChecked(true);
    notificationLayout->addRow(emailCheckbox);
    
    tabWidget->addTab(notificationTab, "Notifications");
    
    // Dialog buttons
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, m_settingsDialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, m_settingsDialog, &QDialog::reject);
    
    QVBoxLayout *mainLayout = new QVBoxLayout();
    mainLayout->addWidget(tabWidget);
    mainLayout->addWidget(buttonBox);
    
    m_settingsDialog->setLayout(mainLayout);
}

void MainWindow::updateCameraView(const cv::Mat& frame)
{
    if (frame.empty()) return;
    
    QImage image = matToQImage(frame);
    m_mainCameraView->setPixmap(QPixmap::fromImage(image).scaled(
        m_mainCameraView->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

QImage MainWindow::matToQImage(const cv::Mat& mat)
{
    // Convert OpenCV Mat to QImage
    if (mat.empty()) {
        return QImage();
    }
    
    if (mat.type() == CV_8UC3) {
        // BGR to RGB conversion
        cv::Mat rgbMat;
        cv::cvtColor(mat, rgbMat, cv::COLOR_BGR2RGB);
        return QImage(rgbMat.data, rgbMat.cols, rgbMat.rows, 
                     static_cast<int>(rgbMat.step), QImage::Format_RGB888);
    } else if (mat.type() == CV_8UC1) {
        // Grayscale image
        return QImage(mat.data, mat.cols, mat.rows, 
                     static_cast<int>(mat.step), QImage::Format_Grayscale8);
    }
    
    return QImage();
}

void MainWindow::showAlert(const QString& title, const QString& message)
{
    QMessageBox::warning(this, title, message);
}

// Menu action slots
void MainWindow::onAboutClicked()
{
    QMessageBox::about(this, "About Human Monitoring System",
        "Human Monitoring System v1.0.0\n\n"
        "A comprehensive system for monitoring humans, detecting falls, "
        "and sending emergency notifications.\n\n"
        " 2025 All Rights Reserved");
}

void MainWindow::onExitClicked()
{
    close();
}

void MainWindow::onSettingsClicked()
{
    if (m_settingsDialog->exec() == QDialog::Accepted) {
        // Apply settings
        // In a real implementation, we would save the settings to the application
    }
}

} // namespace hms

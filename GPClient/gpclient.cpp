#include "gpclient.h"
#include "ui_gpclient.h"
#include "gphelper.h"
#include "vpn_dbus.h"
#include "vpn_json.h"

#include <QApplication>
#include <QCloseEvent>
#include <QMessageBox>
#include <QTimer>
#include <QPushButton>
#include <QIcon>
#include "logging.h"

using namespace gpclient::helper;

ModernGPClient::ModernGPClient(std::shared_ptr<IVpn> vpn, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::GPClient)
    , m_vpn(vpn)
    , m_settings(SettingsManager::instance())
    , m_isAutoConnecting(false)
    , m_isInitialized(false)
    , m_isQuitting(false)
{
    ui->setupUi(this);
    
    setWindowTitle("GlobalProtect");
    setWindowIcon(QIcon(":/images/com.qt.gpclient.svg"));
    
    // Create managers
    m_connectionManager = std::make_shared<ConnectionManager>(m_vpn, this);
    m_authManager = std::make_unique<AuthenticationManager>(this);
    m_systemTray = std::make_unique<SystemTrayManager>(this);
    
    // Auto-connect timer
    m_autoConnectTimer = new QTimer(this);
    m_autoConnectTimer->setSingleShot(true);
    m_autoConnectTimer->setInterval(2000); // 2 second delay for auto-connect
    
    setupUI();
    setupConnections();
    setupSystemTray();
    
    // Initialize from settings
    initializeFromSettings();
    
    m_isInitialized = true;
    
    LOGI << "Modern GP Client initialized successfully";
}

ModernGPClient::~ModernGPClient()
{
    m_isQuitting = true;
    
    // Save current state
    saveWindowGeometry();
    m_settings.sync();
    
    // Clean disconnect if connected
    if (m_connectionManager && m_connectionManager->isConnected()) {
        m_connectionManager->disconnectFromVPN();
    }
    
    delete ui;
}

void ModernGPClient::setupUI()
{
    // Center the window
    moveCenter(this);
    
    // Load settings into UI
    // ui->clientosInput->setText(m_settings.clientOS());
    // ui->osVersionInput->setText(m_settings.osVersion());
    
    // Auto-save settings when fields change
    // connect(ui->clientosInput, &QLineEdit::editingFinished, this, [this]() {
    //     m_settings.setClientOS(ui->clientosInput->text());
    //     m_settings.sync();
    // });
    // connect(ui->osVersionInput, &QLineEdit::editingFinished, this, [this]() {
    //     m_settings.setOsVersion(ui->osVersionInput->text());
    //     m_settings.sync();
    // });
    
    // Initial UI state
    updateUIState();
}

void ModernGPClient::setupConnections()
{
    // UI connections
    connect(ui->connectButton, &QPushButton::clicked, 
            this, &ModernGPClient::onConnectButtonClicked);
    connect(ui->portalInput, &QLineEdit::textChanged, 
            this, &ModernGPClient::onPortalInputChanged);
    connect(ui->portalInput, &QLineEdit::returnPressed, 
            this, &ModernGPClient::onPortalInputReturn);
    
    // Connection manager
    connect(m_connectionManager.get(), &ConnectionManager::stateChanged,
            this, &ModernGPClient::onConnectionStateChanged);
    connect(m_connectionManager.get(), &ConnectionManager::error,
            this, &ModernGPClient::onConnectionError);
    
    // Authentication manager
    connect(m_authManager.get(), &AuthenticationManager::stateChanged,
            this, &ModernGPClient::onAuthenticationStateChanged);
    connect(m_authManager.get(), &AuthenticationManager::authenticationProgress,
            this, &ModernGPClient::onAuthenticationProgress);
    connect(m_authManager.get(), &AuthenticationManager::portalAuthenticationSucceeded,
            this, &ModernGPClient::onPortalAuthSucceeded);
    connect(m_authManager.get(), &AuthenticationManager::gatewayAuthenticationSucceeded,
            this, &ModernGPClient::onGatewayAuthSucceeded);
    connect(m_authManager.get(), &AuthenticationManager::authenticationFailed,
            this, &ModernGPClient::onAuthenticationFailed);
    
    // Settings
    connect(&m_settings, &SettingsManager::portalAddressChanged,
            this, &ModernGPClient::onSettingsChanged);
    connect(&m_settings, &SettingsManager::settingsReset,
            this, &ModernGPClient::onSettingsChanged);
    
    // Auto-connect timer
    connect(m_autoConnectTimer, &QTimer::timeout,
            this, &ModernGPClient::onAutoConnectTimeout);
}

void ModernGPClient::setupSystemTray()
{
    if (!m_systemTray->isSystemTrayAvailable()) {
        LOGW << "System tray not available";
        return;
    }
    
    // System tray connections
    connect(m_systemTray.get(), &SystemTrayManager::showMainWindow,
            this, &ModernGPClient::showMainWindow);
    connect(m_systemTray.get(), &SystemTrayManager::connectRequested,
            this, &ModernGPClient::connectToVPN);
    connect(m_systemTray.get(), &SystemTrayManager::disconnectRequested,
            this, &ModernGPClient::disconnectFromVPN);
    connect(m_systemTray.get(), &SystemTrayManager::gatewayChangeRequested,
            this, &ModernGPClient::onSystemTrayGatewayChange);
    connect(m_systemTray.get(), &SystemTrayManager::resetRequested,
            this, &ModernGPClient::onSystemTrayReset);
    connect(m_systemTray.get(), &SystemTrayManager::quitRequested,
            this, &ModernGPClient::quit);
    
    // Set connection manager for tray
    m_systemTray->setConnectionManager(m_connectionManager);
    m_systemTray->show();
}

void ModernGPClient::initializeFromSettings()
{
    // Restore portal address
    QString portal = m_settings.portalAddress();
    if (!portal.isEmpty()) {
        ui->portalInput->setText(portal);
        m_currentPortal = portal;
    }
    
    // Restore gateways and current gateway
    if (!m_currentPortal.isEmpty()) {
        m_availableGateways = m_settings.gateways(m_currentPortal);
        m_currentGateway = m_settings.currentGateway(m_currentPortal);
        
        if (!m_availableGateways.isEmpty()) {
            m_connectionManager->setGateways(m_availableGateways);
            if (!m_currentGateway.name().isEmpty()) {
                m_connectionManager->setCurrentGateway(m_currentGateway);
            }
        }
    }
    
    // Restore window geometry\n    restoreWindowGeometry();
    
    // Update UI and system tray
    updateUIState();
    updateGatewayMenu();
    
    // Auto-connect if enabled and we have a portal and gateway
    if (m_settings.autoConnect() && !m_currentPortal.isEmpty() && !m_currentGateway.name().isEmpty()) {
        LOGI << "Auto-connect enabled, will connect shortly";
        m_autoConnectTimer->start();
    }
}

void ModernGPClient::setPortalAddress(const QString &address)
{
    ui->portalInput->setText(address);
    onPortalInputChanged();
}

void ModernGPClient::setCurrentGateway(const GPGateway &gateway)
{
    m_currentGateway = gateway;
    m_connectionManager->setCurrentGateway(gateway);
    
    if (!m_currentPortal.isEmpty()) {
        m_settings.setCurrentGateway(m_currentPortal, gateway);
    }
    
    updateGatewayMenu();
}

void ModernGPClient::connectToVPN()
{
    QString portal = ui->portalInput->text().trimmed();
    
    if (portal.isEmpty()) {
        showMainWindow();
        return;
    }
    
    // Save portal address
    m_currentPortal = portal;
    m_settings.setPortalAddress(portal);
    
    // Start authentication process
    if (!m_currentGateway.name().isEmpty()) {
        // Quick connect with saved gateway
        LOGI << "Quick connect to saved gateway: " << m_currentGateway.name();
        GatewayAuthenticatorParams params;
        params.setClientos(m_settings.clientOS());
        m_authManager->authenticateGateway(m_currentGateway.address(), params);
    } else {
        // Start with portal authentication
        LOGI << "Starting portal authentication";
        m_authManager->authenticatePortal(portal);
    }
}

void ModernGPClient::disconnectFromVPN()
{
    if (m_connectionManager) {
        m_connectionManager->disconnectFromVPN();
    }
}

void ModernGPClient::reset()
{
    LOGI << "Resetting client state";
    
    // Disconnect if connected
    if (m_connectionManager && m_connectionManager->isConnected()) {
        m_connectionManager->disconnectFromVPN();
    }
    
    // Reset authentication
    m_authManager->reset();
    
    // Clear UI
    ui->portalInput->clear();
    m_currentPortal.clear();
    m_availableGateways.clear();
    m_currentGateway = GPGateway();
    
    // Reset settings (but not all - keep OS settings etc.)
    m_settings.setPortalAddress("");
    
    updateUIState();
    updateGatewayMenu();
}

void ModernGPClient::updateUIState()
{
    if (!m_isInitialized) {
        return;
    }
    
    // Update based on connection state
    if (m_connectionManager) {
        updateConnectionUI(m_connectionManager->currentState());
    } else {
        updateConnectionUI(ConnectionManager::ConnectionState::Disconnected);
    }
}

void ModernGPClient::updateConnectionUI(ConnectionManager::ConnectionState state)
{
    switch (state) {
        case ConnectionManager::ConnectionState::Disconnected:
            ui->statusLabel->setText("Not Connected");
            ui->statusImage->setStyleSheet("image: url(:/images/disconnected.svg); padding: 15;");
            ui->connectButton->setText("Connect");
            ui->connectButton->setEnabled(true);
            ui->portalInput->setReadOnly(false);
            break;
            
        case ConnectionManager::ConnectionState::Connecting:
            ui->statusLabel->setText("Connecting...");
            ui->statusImage->setStyleSheet("image: url(:/images/connecting.svg); padding: 15;");
            ui->connectButton->setEnabled(false);
            ui->portalInput->setReadOnly(true);
            break;
            
        case ConnectionManager::ConnectionState::Connected:
            ui->statusLabel->setText("Connected");
            if (!m_currentGateway.name().isEmpty()) {
                ui->statusLabel->setText(QString("Connected to %1").arg(m_currentGateway.name()));
            }
            ui->statusImage->setStyleSheet("image: url(:/images/connected.svg); padding: 15;");
            ui->connectButton->setText("Disconnect");
            ui->connectButton->setEnabled(true);
            ui->portalInput->setReadOnly(true);
            break;
            
        case ConnectionManager::ConnectionState::Disconnecting:
            ui->statusLabel->setText("Disconnecting...");
            ui->statusImage->setStyleSheet("image: url(:/images/connecting.svg); padding: 15;");
            ui->connectButton->setEnabled(false);
            ui->portalInput->setReadOnly(true);
            break;
            
        case ConnectionManager::ConnectionState::Error:
            ui->statusLabel->setText("Connection Error");
            ui->statusImage->setStyleSheet("image: url(:/images/disconnected.svg); padding: 15;");
            ui->connectButton->setText("Connect");
            ui->connectButton->setEnabled(true);
            ui->portalInput->setReadOnly(false);
            break;
    }
}

void ModernGPClient::updateGatewayMenu()
{
    if (m_systemTray) {
        m_systemTray->updateGatewayMenu(m_availableGateways, m_currentGateway);
    }
}

void ModernGPClient::showError(const QString &title, const QString &message)
{
    QMessageBox::critical(this, title, message);
    
    // Also show in system tray if available
    if (m_systemTray && m_systemTray->isSystemTrayAvailable()) {
        m_systemTray->showMessage(title, message, QSystemTrayIcon::Critical);
    }
}

void ModernGPClient::showInfo(const QString &title, const QString &message)
{
    if (m_systemTray && m_systemTray->isSystemTrayAvailable()) {
        m_systemTray->showMessage(title, message, QSystemTrayIcon::Information);
    }
}

void ModernGPClient::saveWindowGeometry()
{
    m_settings.setMainWindowGeometry(saveGeometry());
}

void ModernGPClient::restoreWindowGeometry()
{
    QByteArray geometry = m_settings.mainWindowGeometry();
    if (!geometry.isEmpty()) {
        restoreGeometry(geometry);
    }
}

// Event handlers
void ModernGPClient::closeEvent(QCloseEvent *event)
{
    if (!m_isQuitting && m_systemTray && m_systemTray->isSystemTrayAvailable()) {
        hide();
        event->ignore();
    } else {
        saveWindowGeometry();
        event->accept();
    }
}

void ModernGPClient::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);
    
    if (event->type() == QEvent::WindowStateChange) {
        if (isMinimized() && m_systemTray && m_systemTray->isSystemTrayAvailable()) {
            hide();
        }
    }
}

void ModernGPClient::showMainWindow()
{
    show();
    raise();
    activateWindow();
}

void ModernGPClient::quit()
{
    m_isQuitting = true;
    
    if (m_connectionManager && m_connectionManager->isConnected()) {
        m_connectionManager->disconnectFromVPN();
    }
    
    QApplication::quit();
}

// Slot implementations
void ModernGPClient::onConnectButtonClicked()
{
    if (m_connectionManager && m_connectionManager->isConnected()) {
        disconnectFromVPN();
    } else {
        connectToVPN();
    }
}

void ModernGPClient::onPortalInputChanged()
{
    QString newPortal = ui->portalInput->text().trimmed();
    
    if (newPortal != m_currentPortal) {
        m_currentPortal = newPortal;
        
        if (!newPortal.isEmpty()) {
            // Load gateways for this portal
            m_availableGateways = m_settings.gateways(newPortal);
            m_currentGateway = m_settings.currentGateway(newPortal);
            
            if (!m_availableGateways.isEmpty()) {
                m_connectionManager->setGateways(m_availableGateways);
                if (!m_currentGateway.name().isEmpty()) {
                    m_connectionManager->setCurrentGateway(m_currentGateway);
                }
            }
        } else {
            m_availableGateways.clear();
            m_currentGateway = GPGateway();
        }
        
        updateGatewayMenu();
    }
}

void ModernGPClient::onPortalInputReturn()
{
    if (!ui->portalInput->text().trimmed().isEmpty()) {
        connectToVPN();
    }
}

void ModernGPClient::onConnectionStateChanged(ConnectionManager::ConnectionState state)
{
    updateConnectionUI(state);
    
    // Show notifications for important state changes
    switch (state) {
        case ConnectionManager::ConnectionState::Connected:
            showInfo("GlobalProtect", "Connected successfully");
            break;
        case ConnectionManager::ConnectionState::Disconnected:
            if (!m_isQuitting) {
                showInfo("GlobalProtect", "Disconnected");
            }
            break;
        default:
            break;
    }
}

void ModernGPClient::onConnectionError(const QString &error)
{
    LOGE << "Connection error: " << error;
    showError("Connection Failed", error);
    updateUIState();
}

void ModernGPClient::onAuthenticationStateChanged(AuthenticationManager::AuthState state)
{
    // Update UI based on auth state
    switch (state) {
        case AuthenticationManager::AuthState::AuthenticatingPortal:
        case AuthenticationManager::AuthState::AuthenticatingGateway:
            ui->statusLabel->setText("Authenticating...");
            ui->connectButton->setEnabled(false);
            break;
        case AuthenticationManager::AuthState::Failed:
        case AuthenticationManager::AuthState::Idle:
            updateUIState();
            break;
        default:
            break;
    }
}

void ModernGPClient::onAuthenticationProgress(const QString &message)
{
    ui->statusLabel->setText(message);
    LOGI << "Auth progress: " << message;
}

void ModernGPClient::onPortalAuthSucceeded(const PortalConfigResponse &config, const QString &region)
{
    LOGI << "Portal authentication succeeded";
    
    // Update available gateways
    m_availableGateways = config.allGateways();
    m_connectionManager->setGateways(m_availableGateways);
    
    // Save gateways to settings
    if (!m_currentPortal.isEmpty()) {
        m_settings.setGateways(m_currentPortal, m_availableGateways);
    }
    
    // Select preferred gateway if we don't have one
    if (m_currentGateway.name().isEmpty() && !m_availableGateways.isEmpty()) {
        // Use first gateway as default
        m_currentGateway = m_availableGateways.first();
        setCurrentGateway(m_currentGateway);
    }
    
    updateGatewayMenu();
}

void ModernGPClient::onGatewayAuthSucceeded(const QString &authCookie, const QString &username)
{
    LOGI << "Gateway authentication succeeded for user: " << username;
    
    // Now connect to VPN
    if (m_connectionManager && !m_currentGateway.name().isEmpty()) {
        QStringList gatewayAddresses;
        for (const auto &gateway : m_availableGateways) {
            gatewayAddresses.append(gateway.address());
        }
        
        m_connectionManager->connectToVPN(
            m_currentGateway.address(),
            gatewayAddresses,
            username,
            authCookie
        );
    }
}

void ModernGPClient::onAuthenticationFailed(const QString &error)
{
    LOGE << "Authentication failed: " << error;
    showError("Authentication Failed", error);
    updateUIState();
}

void ModernGPClient::onSystemTrayGatewayChange(const GPGateway &gateway)
{
    if (gateway.name() != m_currentGateway.name()) {
        setCurrentGateway(gateway);
        
        // If connected, switch gateway
        if (m_connectionManager && m_connectionManager->isConnected()) {
            m_connectionManager->switchGateway(gateway);
        }
    }
}

void ModernGPClient::onSystemTrayReset()
{
    reset();
}

void ModernGPClient::onSettingsChanged()
{
    updateUIState();
}

void ModernGPClient::onAutoConnectTimeout()
{
    if (!m_isAutoConnecting && !m_currentPortal.isEmpty() && !m_currentGateway.name().isEmpty()) {
        m_isAutoConnecting = true;
        LOGI << "Starting auto-connect";
        connectToVPN();
        m_isAutoConnecting = false;
    }
}
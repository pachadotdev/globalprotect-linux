#include "systemtraymanager.h"
#include <QApplication>
#include <QStyle>
#include "logging.h"

SystemTrayManager::SystemTrayManager(QObject *parent)
    : QObject(parent)
    , m_trayIcon(nullptr)
    , m_contextMenu(nullptr)
    , m_gatewayMenu(nullptr)
    , m_showAction(nullptr)
    , m_connectAction(nullptr)
    , m_resetAction(nullptr)
    , m_quitAction(nullptr)
{
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        createTrayIcon();
        createContextMenu();
        LOGI << "System tray initialized successfully";
    } else {
        LOGW << "System tray is not available on this system";
    }
}

bool SystemTrayManager::isSystemTrayAvailable() const
{
    return QSystemTrayIcon::isSystemTrayAvailable() && m_trayIcon != nullptr;
}

void SystemTrayManager::show()
{
    if (m_trayIcon) {
        m_trayIcon->show();
        LOGI << "System tray icon shown";
    }
}

void SystemTrayManager::hide()
{
    if (m_trayIcon) {
        m_trayIcon->hide();
        LOGI << "System tray icon hidden";
    }
}

void SystemTrayManager::setConnectionManager(std::shared_ptr<ConnectionManager> connectionManager)
{
    // Disconnect previous connection manager if any
    if (m_connectionManager) {
        disconnect(m_connectionManager.get(), nullptr, this, nullptr);
    }
    
    m_connectionManager = connectionManager;
    
    if (m_connectionManager) {
        connect(m_connectionManager.get(), &ConnectionManager::stateChanged,
                this, &SystemTrayManager::onConnectionStateChanged);
        
        // Initialize tray state based on current connection state
        onConnectionStateChanged(m_connectionManager->currentState());
    }
}

void SystemTrayManager::updateGatewayMenu(const QList<GPGateway> &gateways, const GPGateway &current)
{
    m_gateways = gateways;
    m_currentGateway = current;
    
    if (!m_gatewayMenu) {
        return;
    }
    
    // Clear existing gateway menu items
    m_gatewayMenu->clear();
    
    if (gateways.isEmpty()) {
        QAction *noGatewaysAction = m_gatewayMenu->addAction("No gateways available");
        noGatewaysAction->setEnabled(false);
        return;
    }
    
    // Add gateway options
    for (const auto &gateway : gateways) {
        QString actionText = QString("%1 (%2)").arg(gateway.name(), gateway.address());
        QAction *gatewayAction = m_gatewayMenu->addAction(actionText);
        gatewayAction->setData(QVariant::fromValue(gateway));
        gatewayAction->setCheckable(true);
        
        // Check current gateway
        if (gateway.name() == current.name()) {
            gatewayAction->setChecked(true);
        }
    }
    
    LOGI << "Updated gateway menu with " << gateways.size() << " gateways";
}

void SystemTrayManager::showMessage(const QString &title, const QString &message, 
                                   QSystemTrayIcon::MessageIcon icon, int timeout)
{
    if (m_trayIcon && isSystemTrayAvailable()) {
        m_trayIcon->showMessage(title, message, icon, timeout);
    }
}

void SystemTrayManager::createTrayIcon()
{
    m_trayIcon = std::make_unique<QSystemTrayIcon>(this);
    
    // Set initial icon (disconnected state) - use main app icon instead of disconnected
    m_trayIcon->setIcon(QIcon(":/images/com.qt.gpclient.svg"));
    m_trayIcon->setToolTip("GlobalProtect");
    
    connect(m_trayIcon.get(), &QSystemTrayIcon::activated,
            this, &SystemTrayManager::onSystemTrayActivated);
}

void SystemTrayManager::createContextMenu()
{
    if (!m_trayIcon) {
        return;
    }
    
    m_contextMenu = std::make_unique<QMenu>("GlobalProtect");
    
    // Create actions
    m_showAction = m_contextMenu->addAction(QIcon::fromTheme("window-new"), "Show", 
                                           this, &SystemTrayManager::showMainWindow);
    
    m_connectAction = m_contextMenu->addAction(QIcon(":/images/com.qt.gpclient.svg"),
                                              "Connect", this, &SystemTrayManager::connectRequested);
    
    // Create gateway submenu
    m_gatewayMenu = std::make_unique<QMenu>("Switch Gateway");
    m_gatewayMenu->setIcon(QIcon::fromTheme("network-workgroup"));
    connect(m_gatewayMenu.get(), &QMenu::triggered, 
            this, &SystemTrayManager::onGatewayActionTriggered);
    m_contextMenu->addMenu(m_gatewayMenu.get());
    
    m_contextMenu->addSeparator();
    
    m_resetAction = m_contextMenu->addAction(QIcon::fromTheme("edit-clear"), "Reset", 
                                            this, &SystemTrayManager::resetRequested);
    
    m_quitAction = m_contextMenu->addAction(QIcon::fromTheme("application-exit"), "Quit", 
                                           this, &SystemTrayManager::quitRequested);
    
    m_trayIcon->setContextMenu(m_contextMenu.get());
    
    // Initialize menu state
    updateMenuItems(ConnectionManager::ConnectionState::Disconnected);
}

void SystemTrayManager::updateTrayIcon(ConnectionManager::ConnectionState state)
{
    if (!m_trayIcon) {
        return;
    }
    
    QString iconPath;
    QString tooltip;
    
    switch (state) {
        case ConnectionManager::ConnectionState::Disconnected:
            iconPath = ":/images/disconnected.svg";
            tooltip = "GlobalProtect - Disconnected";
            break;
        case ConnectionManager::ConnectionState::Connecting:
        case ConnectionManager::ConnectionState::Disconnecting:
            iconPath = ":/images/connecting.svg";
            tooltip = "GlobalProtect - Connecting...";
            break;
        case ConnectionManager::ConnectionState::Connected:
            iconPath = ":/images/connected.svg";
            tooltip = "GlobalProtect - Connected";
            if (!m_currentGateway.name().isEmpty()) {
                tooltip += QString(" to %1").arg(m_currentGateway.name());
            }
            break;
        case ConnectionManager::ConnectionState::Error:
            iconPath = ":/images/disconnected.svg";
            tooltip = "GlobalProtect - Error";
            break;
    }
    
    m_trayIcon->setIcon(QIcon(iconPath));
    m_trayIcon->setToolTip(tooltip);
}

void SystemTrayManager::updateMenuItems(ConnectionManager::ConnectionState state)
{
    if (!m_connectAction || !m_resetAction || !m_gatewayMenu) {
        return;
    }
    
    switch (state) {
        case ConnectionManager::ConnectionState::Disconnected:
            m_connectAction->setText("Connect");
            m_connectAction->setEnabled(true);
            m_resetAction->setEnabled(true);
            m_gatewayMenu->setEnabled(true);
            break;
        case ConnectionManager::ConnectionState::Connecting:
        case ConnectionManager::ConnectionState::Disconnecting:
            m_connectAction->setEnabled(false);
            m_resetAction->setEnabled(false);
            m_gatewayMenu->setEnabled(false);
            break;
        case ConnectionManager::ConnectionState::Connected:
            m_connectAction->setText("Disconnect");
            m_connectAction->setEnabled(true);
            m_resetAction->setEnabled(false);
            m_gatewayMenu->setEnabled(true);
            break;
        case ConnectionManager::ConnectionState::Error:
            m_connectAction->setText("Connect");
            m_connectAction->setEnabled(true);
            m_resetAction->setEnabled(true);
            m_gatewayMenu->setEnabled(true);
            break;
    }
}

void SystemTrayManager::onSystemTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason) {
        case QSystemTrayIcon::Trigger:
        case QSystemTrayIcon::DoubleClick:
            emit showMainWindow();
            break;
        default:
            break;
    }
}

void SystemTrayManager::onConnectionStateChanged(ConnectionManager::ConnectionState state)
{
    updateTrayIcon(state);
    updateMenuItems(state);
    
    // Show notifications for state changes
    switch (state) {
        case ConnectionManager::ConnectionState::Connected:
            showMessage("GlobalProtect", "Connected successfully", 
                       QSystemTrayIcon::Information);
            break;
        case ConnectionManager::ConnectionState::Disconnected:
            showMessage("GlobalProtect", "Disconnected", 
                       QSystemTrayIcon::Information);
            break;
        case ConnectionManager::ConnectionState::Error:
            showMessage("GlobalProtect", "Connection failed", 
                       QSystemTrayIcon::Critical);
            break;
        default:
            break;
    }
}

void SystemTrayManager::onGatewayActionTriggered(QAction *action)
{
    if (!action) {
        return;
    }
    
    QVariant gatewayData = action->data();
    if (!gatewayData.canConvert<GPGateway>()) {
        return;
    }
    
    GPGateway selectedGateway = gatewayData.value<GPGateway>();
    
    // Don't switch if it's the same gateway
    if (selectedGateway.name() == m_currentGateway.name()) {
        return;
    }
    
    emit gatewayChangeRequested(selectedGateway);
}
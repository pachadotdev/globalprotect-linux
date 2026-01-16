#ifndef GPCLIENT_MODERN_H
#define GPCLIENT_MODERN_H

#include <QMainWindow>
#include <QTimer>
#include <memory>

#include "connectionmanager.h"
#include "authenticationmanager.h"
#include "systemtraymanager.h"
#include "settingsmanager.h"
#include "vpn.h"
#include "gpgateway.h"

QT_BEGIN_NAMESPACE
namespace Ui { class GPClient; }
QT_END_NAMESPACE

class ModernGPClient : public QMainWindow
{
    Q_OBJECT

public:
    explicit ModernGPClient(std::shared_ptr<IVpn> vpn, QWidget *parent = nullptr);
    ~ModernGPClient();

    void setPortalAddress(const QString &address);
    void setCurrentGateway(const GPGateway &gateway);
    void connectToVPN();
    void disconnectFromVPN();
    void reset();

protected:
    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent *event) override;

public slots:
    void showMainWindow();
    void quit();

private slots:
    // UI Events
    void onConnectButtonClicked();
    void onPortalInputChanged();
    void onPortalInputReturn();

    // Connection Manager Events
    void onConnectionStateChanged(ConnectionManager::ConnectionState state);
    void onConnectionError(const QString &error);

    // Authentication Manager Events
    void onAuthenticationStateChanged(AuthenticationManager::AuthState state);
    void onAuthenticationProgress(const QString &message);
    void onPortalAuthSucceeded(const PortalConfigResponse &config, const QString &region);
    void onGatewayAuthSucceeded(const QString &authCookie, const QString &username);
    void onAuthenticationFailed(const QString &error);

    // System Tray Events
    void onSystemTrayGatewayChange(const GPGateway &gateway);
    void onSystemTrayReset();

    // Settings Events
    void onSettingsChanged();

    // Auto-connect timer
    void onAutoConnectTimeout();

private:
    void setupUI();
    void setupConnections();
    void setupSystemTray();
    void initializeFromSettings();
    
    void updateUIState();
    void updateConnectionUI(ConnectionManager::ConnectionState state);
    void updateGatewayMenu();
    
    void showError(const QString &title, const QString &message);
    void showInfo(const QString &title, const QString &message);
    
    void saveWindowGeometry();
    void restoreWindowGeometry();
    
    // Core components
    Ui::GPClient *ui;
    std::shared_ptr<IVpn> m_vpn;
    std::shared_ptr<ConnectionManager> m_connectionManager;
    std::unique_ptr<AuthenticationManager> m_authManager;
    std::unique_ptr<SystemTrayManager> m_systemTray;
    SettingsManager &m_settings;
    
    // UI State
    QString m_currentPortal;
    QList<GPGateway> m_availableGateways;
    GPGateway m_currentGateway;
    
    // Auto-connect functionality
    QTimer *m_autoConnectTimer;
    bool m_isAutoConnecting;
    
    // State tracking
    bool m_isInitialized;
    bool m_isQuitting;
};

#endif // GPCLIENT_MODERN_H
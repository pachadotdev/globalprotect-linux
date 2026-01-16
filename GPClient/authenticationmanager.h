#ifndef AUTHENTICATIONMANAGER_H
#define AUTHENTICATIONMANAGER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QTimer>
#include <memory>
#include "portalconfigresponse.h"
#include "gatewayauthenticatorparams.h"
#include "gpgateway.h"

class PortalAuthenticator;
class GatewayAuthenticator;

class AuthenticationManager : public QObject
{
    Q_OBJECT

public:
    enum class AuthState {
        Idle,
        AuthenticatingPortal,
        AuthenticatingGateway,
        Authenticated,
        Failed
    };
    Q_ENUM(AuthState)

    explicit AuthenticationManager(QObject *parent = nullptr);
    ~AuthenticationManager();

    AuthState currentState() const { return m_currentState; }
    bool isAuthenticated() const { return m_currentState == AuthState::Authenticated; }
    
    // Get current authentication data
    QString currentAuthCookie() const { return m_authCookie; }
    QString currentUsername() const { return m_username; }
    PortalConfigResponse portalConfig() const { return m_portalConfig; }

public slots:
    void authenticatePortal(const QString &portalAddress);
    void authenticateGateway(const QString &gatewayAddress, 
                           const GatewayAuthenticatorParams &params);
    void authenticateGatewayDirect(const QString &gatewayAddress);
    void reset();

signals:
    void stateChanged(AuthState newState);
    void portalAuthenticationSucceeded(const PortalConfigResponse &config, const QString &region);
    void gatewayAuthenticationSucceeded(const QString &authCookie, const QString &username);
    void authenticationFailed(const QString &errorMessage);
    void authenticationProgress(const QString &message);

private slots:
    void onPortalAuthSuccess(const PortalConfigResponse &response, const QString &region);
    void onPortalAuthFailed(const QString &errorMessage);
    void onPortalPreloginFailed(const QString &errorMessage);
    void onPortalConfigFailed(const QString &errorMessage);
    
    void onGatewayAuthSuccess(const QString &authCookie);
    void onGatewayAuthFailed(const QString &errorMessage);

private:
    void setState(AuthState newState);
    void cleanupCurrentAuth();
    GPGateway filterPreferredGateway(const QList<GPGateway> &gateways, const QString &region) const;

    AuthState m_currentState;
    QString m_portalAddress;
    QString m_gatewayAddress;
    QString m_authCookie;
    QString m_username;
    PortalConfigResponse m_portalConfig;
    
    // Current authenticators
    std::unique_ptr<PortalAuthenticator> m_portalAuth;
    std::unique_ptr<GatewayAuthenticator> m_gatewayAuth;
    
    // Network manager for requests
    std::unique_ptr<QNetworkAccessManager> m_networkManager;
    
    // Timeout management
    QTimer *m_timeoutTimer;
    static constexpr int AUTH_TIMEOUT_MS = 60000; // 60 seconds
};

#endif // AUTHENTICATIONMANAGER_H
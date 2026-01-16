#ifndef CONNECTIONMANAGER_H
#define CONNECTIONMANAGER_H

#include <QObject>
#include <QTimer>
#include <QStateMachine>
#include <QState>
#include <memory>
#include "vpn.h"
#include "gpgateway.h"
#include "portalconfigresponse.h"

class ConnectionManager : public QObject
{
    Q_OBJECT

public:
    enum class ConnectionState {
        Disconnected,
        Connecting,
        Connected,
        Disconnecting,
        Error
    };
    Q_ENUM(ConnectionState)

    explicit ConnectionManager(std::shared_ptr<IVpn> vpn, QObject *parent = nullptr);
    ~ConnectionManager() = default;

    ConnectionState currentState() const { return m_currentState; }
    bool isConnected() const { return m_currentState == ConnectionState::Connected; }
    
    void setGateways(const QList<GPGateway> &gateways);
    void setCurrentGateway(const GPGateway &gateway);
    GPGateway currentGateway() const { return m_currentGateway; }
    QList<GPGateway> availableGateways() const { return m_gateways; }

public slots:
    void connectToVPN(const QString &gatewayAddress, const QStringList &allGateways, 
                     const QString &username, const QString &authCookie);
    void disconnectFromVPN();
    void switchGateway(const GPGateway &newGateway);

signals:
    void stateChanged(ConnectionState newState);
    void connected();
    void disconnected();
    void error(const QString &errorMessage);
    void logAvailable(const QString &log);
    void gatewaySwitched(const GPGateway &newGateway);
    
    // State machine transition triggers
    void requestConnect();
    void requestDisconnect();

private slots:
    void onVpnConnected();
    void onVpnDisconnected();
    void onVpnError(const QString &errorMessage);
    void onVpnLogAvailable(const QString &log);
    void onConnectionTimeout();

private:
    void setState(ConnectionState newState);
    void setupStateMachine();

    std::shared_ptr<IVpn> m_vpn;
    ConnectionState m_currentState;
    GPGateway m_currentGateway;
    QList<GPGateway> m_gateways;
    QTimer *m_connectionTimer;
    bool m_isSwitchingGateway;
    QString m_lastError;
    
    // State machine for connection management
    std::unique_ptr<QStateMachine> m_stateMachine;
    QState *m_disconnectedState;
    QState *m_connectingState;
    QState *m_connectedState;
    QState *m_disconnectingState;
    QState *m_errorState;
};

#endif // CONNECTIONMANAGER_H
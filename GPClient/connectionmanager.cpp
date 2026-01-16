#include "connectionmanager.h"
#include <QStateMachine>
#include <QTimer>
#include "logging.h"
#include "vpn_dbus.h"
#include "vpn_json.h"

ConnectionManager::ConnectionManager(std::shared_ptr<IVpn> vpn, QObject *parent)
    : QObject(parent)
    , m_vpn(vpn)
    , m_currentState(ConnectionState::Disconnected)
    , m_connectionTimer(new QTimer(this))
    , m_isSwitchingGateway(false)
    , m_stateMachine(std::make_unique<QStateMachine>(this))
{
    m_connectionTimer->setSingleShot(true);
    m_connectionTimer->setInterval(30000); // 30 second timeout
    connect(m_connectionTimer, &QTimer::timeout, this, &ConnectionManager::onConnectionTimeout);

    setupStateMachine();
    
    if (m_vpn) {
        // Connect to VPN signals based on type
        if (auto vpnDbus = std::dynamic_pointer_cast<VpnDbus>(m_vpn)) {
            connect(vpnDbus.get(), &VpnDbus::connected, this, &ConnectionManager::onVpnConnected);
            connect(vpnDbus.get(), &VpnDbus::disconnected, this, &ConnectionManager::onVpnDisconnected);
            connect(vpnDbus.get(), &VpnDbus::error, this, &ConnectionManager::onVpnError);
            connect(vpnDbus.get(), &VpnDbus::logAvailable, this, &ConnectionManager::onVpnLogAvailable);
        } else if (auto vpnJson = std::dynamic_pointer_cast<VpnJson>(m_vpn)) {
            connect(vpnJson.get(), &VpnJson::connected, this, &ConnectionManager::onVpnConnected);
            connect(vpnJson.get(), &VpnJson::disconnected, this, &ConnectionManager::onVpnDisconnected);
            connect(vpnJson.get(), &VpnJson::error, this, &ConnectionManager::onVpnError);
            connect(vpnJson.get(), &VpnJson::logAvailable, this, &ConnectionManager::onVpnLogAvailable);
        }
    }

    m_stateMachine->start();
}

void ConnectionManager::setupStateMachine()
{
    // Create states
    m_disconnectedState = new QState(m_stateMachine.get());
    m_connectingState = new QState(m_stateMachine.get());
    m_connectedState = new QState(m_stateMachine.get());
    m_disconnectingState = new QState(m_stateMachine.get());
    m_errorState = new QState(m_stateMachine.get());

    // Set initial state
    m_stateMachine->setInitialState(m_disconnectedState);

    // Define transitions using proper signals
    m_disconnectedState->addTransition(this, &ConnectionManager::requestConnect, m_connectingState);
    m_connectingState->addTransition(this, &ConnectionManager::connected, m_connectedState);
    m_connectingState->addTransition(this, &ConnectionManager::error, m_errorState);
    m_connectedState->addTransition(this, &ConnectionManager::requestDisconnect, m_disconnectingState);
    m_connectedState->addTransition(this, &ConnectionManager::error, m_errorState);
    m_disconnectingState->addTransition(this, &ConnectionManager::disconnected, m_disconnectedState);
    m_errorState->addTransition(this, &ConnectionManager::disconnected, m_disconnectedState);

    // Connect state changes to our state tracking
    connect(m_disconnectedState, &QState::entered, this, [this]() { setState(ConnectionState::Disconnected); });
    connect(m_connectingState, &QState::entered, this, [this]() { setState(ConnectionState::Connecting); });
    connect(m_connectedState, &QState::entered, this, [this]() { setState(ConnectionState::Connected); });
    connect(m_disconnectingState, &QState::entered, this, [this]() { setState(ConnectionState::Disconnecting); });
    connect(m_errorState, &QState::entered, this, [this]() { setState(ConnectionState::Error); });
}

void ConnectionManager::setState(ConnectionState newState)
{
    if (m_currentState != newState) {
        m_currentState = newState;
        emit stateChanged(newState);
        
        LOGI << "Connection state changed to: " << static_cast<int>(newState);
    }
}

void ConnectionManager::setGateways(const QList<GPGateway> &gateways)
{
    m_gateways = gateways;
    LOGI << "Updated gateway list with " << gateways.size() << " gateways";
}

void ConnectionManager::setCurrentGateway(const GPGateway &gateway)
{
    m_currentGateway = gateway;
    LOGI << "Current gateway set to: " << gateway.name() << " (" << gateway.address() << ")";
}

void ConnectionManager::connectToVPN(const QString &gatewayAddress, const QStringList &allGateways, 
                                   const QString &username, const QString &authCookie)
{
    if (!m_vpn) {
        LOGE << "VPN interface is null";
        emit error("VPN interface not available");
        return;
    }

    if (m_currentState != ConnectionState::Disconnected) {
        LOGW << "Attempted to connect while not in disconnected state";
        return;
    }

    LOGI << "Connecting to VPN gateway: " << gatewayAddress;
    emit requestConnect();  // Trigger state machine transition
    m_connectionTimer->start();
    
    try {
        m_vpn->connect(gatewayAddress, allGateways, username, authCookie);
    } catch (const std::exception &e) {
        m_connectionTimer->stop();
        QString errorMsg = QString("Failed to connect: %1").arg(e.what());
        LOGE << errorMsg;
        emit error(errorMsg);
    }
}

void ConnectionManager::disconnectFromVPN()
{
    if (!m_vpn) {
        LOGE << "VPN interface is null";
        emit disconnected();
        return;
    }

    if (m_currentState == ConnectionState::Disconnected) {
        LOGW << "Attempted to disconnect while already disconnected";
        return;
    }

    LOGI << "Disconnecting from VPN";
    emit requestDisconnect();  // Trigger state machine transition
    m_connectionTimer->stop();
    
    try {
        m_vpn->disconnect();
    } catch (const std::exception &e) {
        QString errorMsg = QString("Failed to disconnect cleanly: %1").arg(e.what());
        LOGW << errorMsg;
        // Still emit disconnected to reset state
        emit disconnected();
    }
}

void ConnectionManager::switchGateway(const GPGateway &newGateway)
{
    if (newGateway.name() == m_currentGateway.name()) {
        LOGI << "Already connected to gateway: " << newGateway.name();
        return;
    }

    LOGI << "Switching gateway from " << m_currentGateway.name() << " to " << newGateway.name();
    
    m_isSwitchingGateway = true;
    setCurrentGateway(newGateway);
    
    if (m_currentState == ConnectionState::Connected) {
        disconnectFromVPN();
    }
}

void ConnectionManager::onVpnConnected()
{
    m_connectionTimer->stop();
    m_lastError.clear();
    
    if (m_isSwitchingGateway) {
        m_isSwitchingGateway = false;
        emit gatewaySwitched(m_currentGateway);
    }
    
    emit connected();
}

void ConnectionManager::onVpnDisconnected()
{
    m_connectionTimer->stop();
    
    if (m_isSwitchingGateway) {
        // If we were switching gateways, attempt to reconnect to the new one
        // This would need to be handled by the caller with stored auth info
        m_isSwitchingGateway = false;
    }
    
    emit disconnected();
}

void ConnectionManager::onVpnError(const QString &errorMessage)
{
    m_connectionTimer->stop();
    m_lastError = errorMessage;
    LOGE << "VPN Error: " << errorMessage;
    emit error(errorMessage);
}

void ConnectionManager::onVpnLogAvailable(const QString &log)
{
    emit logAvailable(log);
}

void ConnectionManager::onConnectionTimeout()
{
    LOGE << "Connection timeout occurred";
    if (m_vpn) {
        m_vpn->disconnect();
    }
    emit error("Connection timeout");
}
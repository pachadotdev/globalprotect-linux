#include "authenticationmanager.h"
#include "portalauthenticator.h"
#include "gatewayauthenticator.h"
#include "gphelper.h"
#include <QNetworkAccessManager>
#include <QTimer>
#include "logging.h"

using namespace gpclient::helper;

AuthenticationManager::AuthenticationManager(QObject *parent)
    : QObject(parent)
    , m_currentState(AuthState::Idle)
    , m_networkManager(std::make_unique<QNetworkAccessManager>(this))
    , m_timeoutTimer(new QTimer(this))
{
    m_timeoutTimer->setSingleShot(true);
    m_timeoutTimer->setInterval(AUTH_TIMEOUT_MS);
    connect(m_timeoutTimer, &QTimer::timeout, this, [this]() {
        LOGE << "Authentication timeout occurred";
        setState(AuthState::Failed);
        emit authenticationFailed("Authentication timeout");
        cleanupCurrentAuth();
    });
}

AuthenticationManager::~AuthenticationManager()
{
    cleanupCurrentAuth();
    m_timeoutTimer->stop();
}

void AuthenticationManager::setState(AuthState newState)
{
    if (m_currentState != newState) {
        m_currentState = newState;
        emit stateChanged(newState);
        LOGI << "Authentication state changed to: " << static_cast<int>(newState);
    }
}

void AuthenticationManager::authenticatePortal(const QString &portalAddress)
{
    if (m_currentState != AuthState::Idle) {
        LOGW << "Authentication already in progress";
        return;
    }

    LOGI << "Starting portal authentication for: " << portalAddress;
    m_portalAddress = portalAddress;
    cleanupCurrentAuth();
    
    setState(AuthState::AuthenticatingPortal);
    emit authenticationProgress("Authenticating with portal...");
    
    try {
        m_portalAuth = std::make_unique<PortalAuthenticator>(
            portalAddress, 
            settings::get("clientos", "Linux").toString()
        );
        
        connect(m_portalAuth.get(), &PortalAuthenticator::success, 
                this, &AuthenticationManager::onPortalAuthSuccess);
        connect(m_portalAuth.get(), &PortalAuthenticator::fail, 
                this, &AuthenticationManager::onPortalAuthFailed);
        connect(m_portalAuth.get(), &PortalAuthenticator::preloginFailed, 
                this, &AuthenticationManager::onPortalPreloginFailed);
        connect(m_portalAuth.get(), &PortalAuthenticator::portalConfigFailed, 
                this, &AuthenticationManager::onPortalConfigFailed);
        
        m_timeoutTimer->start();
        m_portalAuth->authenticate();
        
    } catch (const std::exception &e) {
        LOGE << "Failed to create portal authenticator: " << e.what();
        setState(AuthState::Failed);
        emit authenticationFailed(QString("Failed to initialize portal authentication: %1").arg(e.what()));
    }
}

void AuthenticationManager::authenticateGateway(const QString &gatewayAddress, 
                                               const GatewayAuthenticatorParams &params)
{
    if (m_currentState != AuthState::Idle) {
        LOGW << "Authentication already in progress";
        return;
    }

    LOGI << "Starting gateway authentication for: " << gatewayAddress;
    m_gatewayAddress = gatewayAddress;
    cleanupCurrentAuth();
    
    setState(AuthState::AuthenticatingGateway);
    emit authenticationProgress("Authenticating with gateway...");
    
    try {
        m_gatewayAuth = std::make_unique<GatewayAuthenticator>(gatewayAddress, params);
        
        connect(m_gatewayAuth.get(), &GatewayAuthenticator::success, 
                this, &AuthenticationManager::onGatewayAuthSuccess);
        connect(m_gatewayAuth.get(), &GatewayAuthenticator::fail, 
                this, &AuthenticationManager::onGatewayAuthFailed);
        
        m_timeoutTimer->start();
        m_gatewayAuth->authenticate();
        
    } catch (const std::exception &e) {
        LOGE << "Failed to create gateway authenticator: " << e.what();
        setState(AuthState::Failed);
        emit authenticationFailed(QString("Failed to initialize gateway authentication: %1").arg(e.what()));
    }
}

void AuthenticationManager::authenticateGatewayDirect(const QString &gatewayAddress)
{
    LOGI << "Starting direct gateway authentication (treating portal as gateway)";
    
    // Create gateway from portal address
    GPGateway gateway;
    gateway.setName(gatewayAddress);
    gateway.setAddress(gatewayAddress);
    
    // Create default params
    GatewayAuthenticatorParams params;
    params.setClientos(settings::get("clientos", "Linux").toString());
    
    authenticateGateway(gatewayAddress, params);
}

void AuthenticationManager::reset()
{
    LOGI << "Resetting authentication manager";
    
    m_timeoutTimer->stop();
    cleanupCurrentAuth();
    
    m_portalAddress.clear();
    m_gatewayAddress.clear();
    m_authCookie.clear();
    m_username.clear();
    m_portalConfig = PortalConfigResponse();
    
    setState(AuthState::Idle);
}

void AuthenticationManager::cleanupCurrentAuth()
{
    m_portalAuth.reset();
    m_gatewayAuth.reset();
}

void AuthenticationManager::onPortalAuthSuccess(const PortalConfigResponse &response, const QString &region)
{
    m_timeoutTimer->stop();
    
    LOGI << "Portal authentication succeeded";
    m_portalConfig = response;
    
    // Check if we have gateways
    if (response.allGateways().isEmpty()) {
        LOGI << "No gateways in portal config, treating portal as gateway";
        // Treat the portal as a gateway and authenticate directly
        setState(AuthState::Idle);
        authenticateGatewayDirect(m_portalAddress);
        return;
    }
    
    // Select preferred gateway
    GPGateway preferredGateway = filterPreferredGateway(response.allGateways(), region);
    
    // Now authenticate with the selected gateway
    GatewayAuthenticatorParams params = GatewayAuthenticatorParams::fromPortalConfigResponse(response);
    params.setClientos(settings::get("clientos", "Linux").toString());
    
    cleanupCurrentAuth();
    setState(AuthState::Idle);
    
    emit portalAuthenticationSucceeded(response, region);
    
    // Continue with gateway authentication
    authenticateGateway(preferredGateway.address(), params);
}

void AuthenticationManager::onPortalAuthFailed(const QString &errorMessage)
{
    m_timeoutTimer->stop();
    
    LOGE << "Portal authentication failed: " << errorMessage;
    setState(AuthState::Failed);
    cleanupCurrentAuth();
    emit authenticationFailed(QString("Portal authentication failed: %1").arg(errorMessage));
}

void AuthenticationManager::onPortalPreloginFailed(const QString &errorMessage)
{
    m_timeoutTimer->stop();
    
    LOGI << "Portal prelogin failed, treating as gateway: " << errorMessage;
    cleanupCurrentAuth();
    setState(AuthState::Idle);
    
    // Try direct gateway authentication
    authenticateGatewayDirect(m_portalAddress);
}

void AuthenticationManager::onPortalConfigFailed(const QString &errorMessage)
{
    m_timeoutTimer->stop();
    
    LOGI << "Portal config failed, treating as gateway: " << errorMessage;
    cleanupCurrentAuth();
    setState(AuthState::Idle);
    
    // Try direct gateway authentication
    authenticateGatewayDirect(m_portalAddress);
}

void AuthenticationManager::onGatewayAuthSuccess(const QString &authCookie)
{
    m_timeoutTimer->stop();
    
    LOGI << "Gateway authentication succeeded";
    m_authCookie = authCookie;
    
    // Extract username from portal config if available
    if (!m_portalConfig.username().isEmpty()) {
        m_username = m_portalConfig.username();
    }
    
    setState(AuthState::Authenticated);
    cleanupCurrentAuth();
    emit gatewayAuthenticationSucceeded(authCookie, m_username);
}

void AuthenticationManager::onGatewayAuthFailed(const QString &errorMessage)
{
    m_timeoutTimer->stop();
    
    LOGE << "Gateway authentication failed: " << errorMessage;
    setState(AuthState::Failed);
    cleanupCurrentAuth();
    emit authenticationFailed(QString("Gateway authentication failed: %1").arg(errorMessage));
}

GPGateway AuthenticationManager::filterPreferredGateway(const QList<GPGateway> &gateways, const QString &region) const
{
    if (gateways.isEmpty()) {
        return GPGateway();
    }
    
    // If only one gateway, use it
    if (gateways.size() == 1) {
        return gateways.first();
    }
    
    // Try to find a gateway that matches the region
    if (!region.isEmpty()) {
        for (const auto &gateway : gateways) {
            if (gateway.name().contains(region, Qt::CaseInsensitive)) {
                LOGI << "Selected gateway by region: " << gateway.name();
                return gateway;
            }
        }
    }
    
    // Fall back to first gateway
    LOGI << "Using first available gateway: " << gateways.first().name();
    return gateways.first();
}
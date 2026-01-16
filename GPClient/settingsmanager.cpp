#include "settingsmanager.h"
#include "gphelper.h"
#include <QStandardPaths>
#include <QDir>
#include <QSysInfo>
#include <QMutexLocker>
#include "logging.h"

using namespace gpclient::helper;

SettingsManager& SettingsManager::instance()
{
    static SettingsManager instance;
    return instance;
}

SettingsManager::SettingsManager(QObject *parent)
    : QObject(parent)
{
    // Create settings with application-specific location
    QString configPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    QDir configDir(configPath);
    if (!configDir.exists()) {
        configDir.mkpath(configPath);
    }
    
    QString settingsFile = configPath + "/globalprotect/settings.conf";
    m_settings = std::make_unique<QSettings>(settingsFile, QSettings::IniFormat);
    
    initializeDefaults();
    
    LOGI << "Settings manager initialized with file: " << settingsFile;
}

void SettingsManager::initializeDefaults()
{
    QMutexLocker locker(&m_mutex);
    
    // Set default values if they don't exist
    if (!m_settings->contains("client/os")) {
        m_settings->setValue("client/os", DEFAULT_CLIENT_OS);
    }
    
    if (!m_settings->contains("client/osVersion")) {
        m_settings->setValue("client/osVersion", QSysInfo::prettyProductName());
    }
    
    if (!m_settings->contains("ui/startMinimized")) {
        m_settings->setValue("ui/startMinimized", DEFAULT_START_MINIMIZED);
    }
    
    if (!m_settings->contains("connection/autoConnect")) {
        m_settings->setValue("connection/autoConnect", DEFAULT_AUTO_CONNECT);
    }
    
    if (!m_settings->contains("logging/level")) {
        m_settings->setValue("logging/level", DEFAULT_LOG_LEVEL);
    }
    
    if (!m_settings->contains("logging/toFile")) {
        m_settings->setValue("logging/toFile", DEFAULT_LOG_TO_FILE);
    }
    
    if (!m_settings->contains("logging/filePath")) {
        QString defaultLogPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/logs/gpclient.log";
        m_settings->setValue("logging/filePath", defaultLogPath);
    }
}

QString SettingsManager::portalAddress() const
{
    QMutexLocker locker(&m_mutex);
    return m_settings->value("connection/portal", "").toString();
}

void SettingsManager::setPortalAddress(const QString &address)
{
    QMutexLocker locker(&m_mutex);
    QString currentAddress = m_settings->value("connection/portal", "").toString();
    if (currentAddress != address) {
        m_settings->setValue("connection/portal", address);
        emit portalAddressChanged(address);
    }
}

QString SettingsManager::clientOS() const
{
    QMutexLocker locker(&m_mutex);
    return m_settings->value("client/os", DEFAULT_CLIENT_OS).toString();
}

void SettingsManager::setClientOS(const QString &os)
{
    QMutexLocker locker(&m_mutex);
    QString currentOS = m_settings->value("client/os", DEFAULT_CLIENT_OS).toString();
    if (currentOS != os) {
        m_settings->setValue("client/os", os);
        emit clientOSChanged(os);
    }
}

QString SettingsManager::osVersion() const
{
    QMutexLocker locker(&m_mutex);
    return m_settings->value("client/osVersion", QSysInfo::prettyProductName()).toString();
}

void SettingsManager::setOsVersion(const QString &version)
{
    QMutexLocker locker(&m_mutex);
    m_settings->setValue("client/osVersion", version);
}

bool SettingsManager::startMinimized() const
{
    QMutexLocker locker(&m_mutex);
    return m_settings->value("ui/startMinimized", DEFAULT_START_MINIMIZED).toBool();
}

void SettingsManager::setStartMinimized(bool minimized)
{
    QMutexLocker locker(&m_mutex);
    m_settings->setValue("ui/startMinimized", minimized);
}

bool SettingsManager::autoConnect() const
{
    QMutexLocker locker(&m_mutex);
    return m_settings->value("connection/autoConnect", DEFAULT_AUTO_CONNECT).toBool();
}

void SettingsManager::setAutoConnect(bool autoConnect)
{
    QMutexLocker locker(&m_mutex);
    m_settings->setValue("connection/autoConnect", autoConnect);
}

QString SettingsManager::gatewaysKey(const QString &portalAddress) const
{
    return QString("gateways/%1/list").arg(QString(portalAddress).replace("/", "_"));
}

QString SettingsManager::selectedGatewayKey(const QString &portalAddress) const
{
    return QString("gateways/%1/selected").arg(QString(portalAddress).replace("/", "_"));
}

QList<GPGateway> SettingsManager::gateways(const QString &portalAddress) const
{
    QMutexLocker locker(&m_mutex);
    QList<GPGateway> gatewayList;
    
    QString key = gatewaysKey(portalAddress);
    QString gatewayData = m_settings->value(key, "").toString();
    
    if (!gatewayData.isEmpty()) {
        gatewayList = GPGateway::fromJson(gatewayData);
    }
    
    return gatewayList;
}

void SettingsManager::setGateways(const QString &portalAddress, const QList<GPGateway> &gateways)
{
    QMutexLocker locker(&m_mutex);
    QString key = gatewaysKey(portalAddress);
    QList<GPGateway> mutableGateways = gateways; // Make mutable copy
    QString serializedGateways = GPGateway::serialize(mutableGateways);
    m_settings->setValue(key, serializedGateways);
    
    LOGI << "Stored " << gateways.size() << " gateways for portal: " << portalAddress;
}

GPGateway SettingsManager::currentGateway(const QString &portalAddress) const
{
    QMutexLocker locker(&m_mutex);
    QString key = selectedGatewayKey(portalAddress);
    QString selectedGatewayName = m_settings->value(key, "").toString();
    
    if (selectedGatewayName.isEmpty()) {
        return GPGateway();
    }
    
    // Find the gateway by name
    QList<GPGateway> gatewayList = gateways(portalAddress);
    for (const auto &gateway : gatewayList) {
        if (gateway.name() == selectedGatewayName) {
            return gateway;
        }
    }
    
    return GPGateway();
}

void SettingsManager::setCurrentGateway(const QString &portalAddress, const GPGateway &gateway)
{
    QMutexLocker locker(&m_mutex);
    QString key = selectedGatewayKey(portalAddress);
    m_settings->setValue(key, gateway.name());
    
    LOGI << "Set current gateway to: " << gateway.name() << " for portal: " << portalAddress;
}

bool SettingsManager::hasStoredCredentials() const
{
    QString username, password;
    return getStoredCredentials(username, password) && !username.isEmpty() && !password.isEmpty();
}

QString SettingsManager::storedUsername() const
{
    QString username, password;
    if (getStoredCredentials(username, password)) {
        return username;
    }
    return QString();
}

void SettingsManager::storeCredentials(const QString &username, const QString &password)
{
    try {
        settings::secureSave("username", username);
        settings::secureSave("password", password);
        LOGI << "Credentials stored securely for user: " << username;
    } catch (const std::exception &e) {
        LOGE << "Failed to store credentials: " << e.what();
    }
}

void SettingsManager::clearStoredCredentials()
{
    try {
        settings::secureSave("username", "");
        settings::secureSave("password", "");
        LOGI << "Stored credentials cleared";
    } catch (const std::exception &e) {
        LOGW << "Failed to clear credentials: " << e.what();
    }
}

bool SettingsManager::getStoredCredentials(QString &username, QString &password) const
{
    try {
        bool hasUsername = settings::secureGet("username", username);
        bool hasPassword = settings::secureGet("password", password);
        return hasUsername && hasPassword;
    } catch (const std::exception &e) {
        LOGW << "Failed to retrieve credentials: " << e.what();
        username.clear();
        password.clear();
        return false;
    }
}

QByteArray SettingsManager::mainWindowGeometry() const
{
    QMutexLocker locker(&m_mutex);
    return m_settings->value("ui/mainWindowGeometry", QByteArray()).toByteArray();
}

void SettingsManager::setMainWindowGeometry(const QByteArray &geometry)
{
    QMutexLocker locker(&m_mutex);
    m_settings->setValue("ui/mainWindowGeometry", geometry);
}

int SettingsManager::logLevel() const
{
    QMutexLocker locker(&m_mutex);
    return m_settings->value("logging/level", DEFAULT_LOG_LEVEL).toInt();
}

void SettingsManager::setLogLevel(int level)
{
    QMutexLocker locker(&m_mutex);
    m_settings->setValue("logging/level", level);
}

bool SettingsManager::logToFile() const
{
    QMutexLocker locker(&m_mutex);
    return m_settings->value("logging/toFile", DEFAULT_LOG_TO_FILE).toBool();
}

void SettingsManager::setLogToFile(bool enabled)
{
    QMutexLocker locker(&m_mutex);
    m_settings->setValue("logging/toFile", enabled);
}

QString SettingsManager::logFilePath() const
{
    QMutexLocker locker(&m_mutex);
    QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/logs/gpclient.log";
    return m_settings->value("logging/filePath", defaultPath).toString();
}

void SettingsManager::setLogFilePath(const QString &path)
{
    QMutexLocker locker(&m_mutex);
    m_settings->setValue("logging/filePath", path);
}

void SettingsManager::resetAll()
{
    QMutexLocker locker(&m_mutex);
    
    LOGI << "Resetting all settings";
    
    // Clear all settings but keep the file structure
    m_settings->clear();
    
    // Reinitialize defaults
    initializeDefaults();
    
    // Clear stored credentials
    clearStoredCredentials();
    
    emit settingsReset();
}

void SettingsManager::sync()
{
    QMutexLocker locker(&m_mutex);
    m_settings->sync();
    LOGD << "Settings synchronized to disk";
}
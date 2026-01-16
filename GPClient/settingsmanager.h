#ifndef SETTINGSMANAGER_H
#define SETTINGSMANAGER_H

#include <QObject>
#include <QSettings>
#include <QMutex>
#include <memory>
#include "gpgateway.h"

class SettingsManager : public QObject
{
    Q_OBJECT

public:
    static SettingsManager& instance();
    
    // Application settings
    QString portalAddress() const;
    void setPortalAddress(const QString &address);
    
    QString clientOS() const;
    void setClientOS(const QString &os);
    
    QString osVersion() const;
    void setOsVersion(const QString &version);
    
    bool startMinimized() const;
    void setStartMinimized(bool minimized);
    
    bool autoConnect() const;
    void setAutoConnect(bool autoConnect);
    
    // Gateway management
    QList<GPGateway> gateways(const QString &portalAddress) const;
    void setGateways(const QString &portalAddress, const QList<GPGateway> &gateways);
    
    GPGateway currentGateway(const QString &portalAddress) const;
    void setCurrentGateway(const QString &portalAddress, const GPGateway &gateway);
    
    // Credential management (secure storage)
    bool hasStoredCredentials() const;
    QString storedUsername() const;
    void storeCredentials(const QString &username, const QString &password);
    void clearStoredCredentials();
    bool getStoredCredentials(QString &username, QString &password) const;
    
    // Window geometry
    QByteArray mainWindowGeometry() const;
    void setMainWindowGeometry(const QByteArray &geometry);
    
    // Logging settings
    int logLevel() const;
    void setLogLevel(int level);
    
    bool logToFile() const;
    void setLogToFile(bool enabled);
    
    QString logFilePath() const;
    void setLogFilePath(const QString &path);
    
    // Reset all settings
    void resetAll();
    
    // Sync settings to disk
    void sync();

signals:
    void portalAddressChanged(const QString &address);
    void clientOSChanged(const QString &os);
    void settingsReset();

private:
    explicit SettingsManager(QObject *parent = nullptr);
    ~SettingsManager() = default;
    
    // Prevent copying
    SettingsManager(const SettingsManager&) = delete;
    SettingsManager& operator=(const SettingsManager&) = delete;
    
    void initializeDefaults();
    QString gatewaysKey(const QString &portalAddress) const;
    QString selectedGatewayKey(const QString &portalAddress) const;
    
    std::unique_ptr<QSettings> m_settings;
    mutable QMutex m_mutex;
    
    // Default values
    static constexpr const char* DEFAULT_CLIENT_OS = "Linux";
    static constexpr const char* DEFAULT_LOG_LEVEL = "2"; // Info level
    static constexpr bool DEFAULT_START_MINIMIZED = false;
    static constexpr bool DEFAULT_AUTO_CONNECT = false;
    static constexpr bool DEFAULT_LOG_TO_FILE = false;
};

#endif // SETTINGSMANAGER_H
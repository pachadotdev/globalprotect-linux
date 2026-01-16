#ifndef SYSTEMTRAYMANAGER_H
#define SYSTEMTRAYMANAGER_H

#include <QObject>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <memory>
#include "connectionmanager.h"
#include "gpgateway.h"

class SystemTrayManager : public QObject
{
    Q_OBJECT

public:
    explicit SystemTrayManager(QObject *parent = nullptr);
    ~SystemTrayManager() = default;

    bool isSystemTrayAvailable() const;
    void show();
    void hide();
    
    void setConnectionManager(std::shared_ptr<ConnectionManager> connectionManager);
    void updateGatewayMenu(const QList<GPGateway> &gateways, const GPGateway &current);

public slots:
    void showMessage(const QString &title, const QString &message, 
                    QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::Information,
                    int timeout = 5000);

signals:
    void showMainWindow();
    void connectRequested();
    void disconnectRequested();
    void gatewayChangeRequested(const GPGateway &gateway);
    void resetRequested();
    void quitRequested();

private slots:
    void onSystemTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void onConnectionStateChanged(ConnectionManager::ConnectionState state);
    void onGatewayActionTriggered(QAction *action);

private:
    void createTrayIcon();
    void createContextMenu();
    void updateTrayIcon(ConnectionManager::ConnectionState state);
    void updateMenuItems(ConnectionManager::ConnectionState state);

    std::unique_ptr<QSystemTrayIcon> m_trayIcon;
    std::unique_ptr<QMenu> m_contextMenu;
    std::unique_ptr<QMenu> m_gatewayMenu;
    
    // Menu actions
    QAction *m_showAction;
    QAction *m_connectAction;
    QAction *m_resetAction;
    QAction *m_quitAction;
    
    // Connection manager reference
    std::shared_ptr<ConnectionManager> m_connectionManager;
    
    // Current gateway list for menu updates
    QList<GPGateway> m_gateways;
    GPGateway m_currentGateway;
};

#endif // SYSTEMTRAYMANAGER_H
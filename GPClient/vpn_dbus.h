#ifndef VPN_DBUS_H
#define VPN_DBUS_H
#include "vpn.h"
#include "gpserviceinterface.h"

class VpnDbus : public QObject, public IVpn
{
  Q_OBJECT
  Q_INTERFACES(IVpn)

private:
  com::pacha::qt::GPService *inner;

public:
  VpnDbus(QObject *parent) : QObject(parent) {
    inner = new com::pacha::qt::GPService("com.qt.GPService", "/", QDBusConnection::systemBus(), this);
    
    // Check if the DBus interface is valid before connecting signals
    if (inner->isValid()) {
      QObject::connect(inner, &com::pacha::qt::GPService::connected, this, &VpnDbus::connected);
      QObject::connect(inner, &com::pacha::qt::GPService::disconnected, this, &VpnDbus::disconnected);
      QObject::connect(inner, &com::pacha::qt::GPService::error, this, &VpnDbus::error);
      QObject::connect(inner, &com::pacha::qt::GPService::logAvailable, this, &VpnDbus::logAvailable);
    }
  }

  void connect(const QString &preferredServer, const QList<QString> &servers, const QString &username, const QString &passwd);
  void disconnect();
  int status();

signals: // SIGNALS
  void connected();
  void disconnected();
  void error(QString errorMessage);
  void logAvailable(QString log);
};
#endif

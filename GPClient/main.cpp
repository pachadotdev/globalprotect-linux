#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStandardPaths>
#include <QtCore/QCommandLineParser>
#include <type_traits>
#include <csignal>

#include "logging.h"
#include "singleinstance.h"
#include "signalhandler.h"
#include "gpclient.h"
#include "vpn_dbus.h"
#include "vpn_json.h"
#include "enhancedwebview.h"
#include "version.h"

#define QT_AUTO_SCREEN_SCALE_FACTOR "QT_AUTO_SCREEN_SCALE_FACTOR"

int main(int argc, char *argv[])
{
    LOGI << "GlobalProtect started, version: " << VERSION;

    auto port = QString::fromLocal8Bit(qgetenv(ENV_CDP_PORT));
    auto hidpiSupport = QString::fromLocal8Bit(qgetenv(QT_AUTO_SCREEN_SCALE_FACTOR));

    if (port.isEmpty()) {
        qputenv(ENV_CDP_PORT, "12315");
    }

    if (hidpiSupport.isEmpty()) {
        qputenv(QT_AUTO_SCREEN_SCALE_FACTOR, "1");
    }

    SingleInstance app(argc, argv, "com.qt.gpclient");
    
    // If not primary instance, the other instance was notified, exit
    if (!app.isPrimary()) {
        LOGI << "Another instance is already running, activating it";
        return 0;
    }
    
    // Set application icon globally
    app.setWindowIcon(QIcon(":/images/com.qt.gpclient.svg"));
    
    app.setQuitOnLastWindowClosed(false);

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("server", "The URL of the VPN server. Optional.");
    parser.addPositionalArgument("gateway", "The URL of the specific VPN gateway. Optional.");
    parser.addOptions({
      {"json", "Write the result of the handshake with the GlobalConnect server to stdout as JSON and terminate. Useful for scripting."},
      {"now", "Do not show the dialog with the connect button; connect immediately instead."},
      {"start-minimized", "Launch the client minimized."},
      {"reset", "Reset the client's settings."},
    });
    parser.process(app);

    const auto positional = parser.positionalArguments();

    std::shared_ptr<IVpn> vpn;
    if (parser.isSet("json")) {
        vpn = std::make_shared<VpnJson>(nullptr); // Print to stdout and exit
    } else {
        vpn = std::make_shared<VpnDbus>(nullptr); // Contact GPService daemon via dbus
    }
    ModernGPClient w(vpn);

    if (positional.size() > 0) {
      w.setPortalAddress(positional.at(0));
    }
    if (positional.size() > 1) {
      GPGateway gw;
      gw.setName(positional.at(1));
      gw.setAddress(positional.at(1));
      w.setCurrentGateway(gw);
    }

    QObject::connect(&app, &SingleInstance::instanceStarted, &w, &ModernGPClient::showMainWindow);

    SignalHandler signalHandler;
    signalHandler.watchForSignal(SIGINT);
    signalHandler.watchForSignal(SIGTERM);
    signalHandler.watchForSignal(SIGQUIT);
    signalHandler.watchForSignal(SIGHUP);
    QObject::connect(&signalHandler, &SignalHandler::unixSignal, &w, &ModernGPClient::quit);

    if (parser.isSet("json")) {
        QObject::connect(static_cast<VpnJson*>(vpn.get()), &VpnJson::connected, &w, &ModernGPClient::quit);
    }

    if (parser.isSet("reset")) {
        w.reset();
    }

    if (parser.isSet("now")) {
      w.connectToVPN();
    } else if (parser.isSet("start-minimized")) {
      w.showMinimized();
    } else {
      w.show();
    }

    return app.exec();
}

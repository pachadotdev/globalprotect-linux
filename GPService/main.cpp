#include <QtCore/QCoreApplication>
#include <QtCore/QCommandLineParser>
#include <QtCore/QSocketNotifier>
#include <QtDBus/QtDBus>
#include <csignal>
#include <sys/socket.h>
#include <unistd.h>

#include "gpservice.h"
#include "version.h"

// Simple signal handler for the service
static int s_signalFd[2] = {-1, -1};

static void signalHandler(int signal) {
    ssize_t unused = ::write(s_signalFd[0], &signal, sizeof(signal));
    (void)unused;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName("gpservice");
    app.setApplicationVersion(QString::fromLocal8Bit(VERSION));

    QCommandLineParser parser;
    parser.setApplicationDescription("GlobalProtect openconnect DBus service");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.process(app);

    if (!QDBusConnection::systemBus().isConnected()) {
        qWarning("Cannot connect to the D-Bus session bus.\n"
                 "Please check your system settings and try again.\n");
        return 1;
    }

    GPService service;

    // Setup signal handling using Qt-native approach
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, s_signalFd) == 0) {
        QSocketNotifier *notifier = new QSocketNotifier(s_signalFd[1], QSocketNotifier::Read, &app);
        QObject::connect(notifier, &QSocketNotifier::activated, [&service, notifier]() {
            notifier->setEnabled(false);
            int signal;
            if (::read(s_signalFd[1], &signal, sizeof(signal)) == sizeof(signal)) {
                service.quit();
            }
            notifier->setEnabled(true);
        });

        struct sigaction sa;
        sa.sa_handler = signalHandler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_RESTART;

        sigaction(SIGINT, &sa, nullptr);
        sigaction(SIGTERM, &sa, nullptr);
        sigaction(SIGQUIT, &sa, nullptr);
        sigaction(SIGHUP, &sa, nullptr);
    }

    return app.exec();
}

#ifndef SIGNALHANDLER_H
#define SIGNALHANDLER_H

#include <QObject>
#include <QSocketNotifier>
#include <csignal>
#include <sys/socket.h>
#include <unistd.h>

/**
 * @brief Qt6-native Unix signal handler
 * 
 * Converts Unix signals (SIGINT, SIGTERM, etc.) to Qt signals.
 * Uses the self-pipe trick recommended by Qt documentation.
 */
class SignalHandler : public QObject {
    Q_OBJECT

public:
    explicit SignalHandler(QObject *parent = nullptr)
        : QObject(parent)
        , m_notifier(nullptr)
    {
        // Create the socket pair
        if (::socketpair(AF_UNIX, SOCK_STREAM, 0, s_signalFd) != 0) {
            return;
        }

        // Create notifier for the read end of the pipe
        m_notifier = new QSocketNotifier(s_signalFd[1], QSocketNotifier::Read, this);
        connect(m_notifier, &QSocketNotifier::activated, this, &SignalHandler::handleSignal);

        // Store instance for signal handler
        s_instance = this;
    }

    ~SignalHandler() override {
        s_instance = nullptr;
        if (s_signalFd[0] != -1) {
            ::close(s_signalFd[0]);
            ::close(s_signalFd[1]);
        }
    }

    void watchForSignal(int signal) {
        struct sigaction sa;
        sa.sa_handler = SignalHandler::signalHandler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_RESTART;
        
        if (sigaction(signal, &sa, nullptr) != 0) {
            // Failed to install handler
        }
    }

signals:
    void unixSignal(int signal);

private slots:
    void handleSignal() {
        m_notifier->setEnabled(false);
        
        int signal;
        ssize_t bytesRead = ::read(s_signalFd[1], &signal, sizeof(signal));
        
        if (bytesRead == sizeof(signal)) {
            emit unixSignal(signal);
        }
        
        m_notifier->setEnabled(true);
    }

private:
    static void signalHandler(int signal) {
        // Write the signal number to the pipe - this is async-signal-safe
        if (s_instance) {
            ssize_t unused = ::write(s_signalFd[0], &signal, sizeof(signal));
            (void)unused;
        }
    }

    static int s_signalFd[2];
    static SignalHandler *s_instance;
    QSocketNotifier *m_notifier;
};

// Static members initialization
inline int SignalHandler::s_signalFd[2] = {-1, -1};
inline SignalHandler *SignalHandler::s_instance = nullptr;

#endif // SIGNALHANDLER_H

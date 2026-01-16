#ifndef SINGLEINSTANCE_H
#define SINGLEINSTANCE_H

#include <QApplication>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QCryptographicHash>
#include <QStandardPaths>
#include <QDir>

/**
 * @brief Qt6-native single instance application
 * 
 * Ensures only one instance of the application runs at a time.
 * If another instance is launched, it notifies the primary instance.
 */
class SingleInstance : public QApplication {
    Q_OBJECT

public:
    SingleInstance(int &argc, char **argv, const QString &appId = QString())
        : QApplication(argc, argv)
        , m_isPrimary(false)
        , m_localServer(nullptr)
        , m_lockFile(nullptr)
    {
        QString id = appId.isEmpty() ? applicationName() : appId;
        m_serverName = generateServerName(id);
        
        QString lockPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        QDir().mkpath(lockPath);
        m_lockFile = new QLockFile(lockPath + "/" + m_serverName + ".lock");
        m_lockFile->setStaleLockTime(0);
        
        if (m_lockFile->tryLock(100)) {
            // We are the primary instance
            m_isPrimary = true;
            startServer();
        } else {
            // Another instance is running, notify it
            m_isPrimary = false;
            notifyPrimaryInstance();
        }
    }

    ~SingleInstance() override {
        if (m_localServer) {
            m_localServer->close();
            delete m_localServer;
        }
        if (m_lockFile) {
            m_lockFile->unlock();
            delete m_lockFile;
        }
    }

    bool isPrimary() const { return m_isPrimary; }

signals:
    void instanceStarted();

private:
    void startServer() {
        m_localServer = new QLocalServer(this);
        
        // Remove any stale socket
        QLocalServer::removeServer(m_serverName);
        
        if (m_localServer->listen(m_serverName)) {
            connect(m_localServer, &QLocalServer::newConnection, this, [this]() {
                QLocalSocket *socket = m_localServer->nextPendingConnection();
                if (socket) {
                    socket->waitForReadyRead(1000);
                    socket->deleteLater();
                    emit instanceStarted();
                }
            });
        }
    }

    void notifyPrimaryInstance() {
        QLocalSocket socket;
        socket.connectToServer(m_serverName);
        if (socket.waitForConnected(1000)) {
            socket.write("activate");
            socket.waitForBytesWritten(1000);
            socket.disconnectFromServer();
        }
    }

    QString generateServerName(const QString &appId) {
        QByteArray hash = QCryptographicHash::hash(
            (appId + QString::number(getuid())).toUtf8(),
            QCryptographicHash::Md5
        );
        return "gpclient_" + QString::fromLatin1(hash.toHex().left(16));
    }

    bool m_isPrimary;
    QLocalServer *m_localServer;
    QLockFile *m_lockFile;
    QString m_serverName;
};

#endif // SINGLEINSTANCE_H

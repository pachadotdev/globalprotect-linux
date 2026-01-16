#ifndef LOGGING_H
#define LOGGING_H

#include <QDebug>
#include <QDateTime>
#include <QTextStream>

// Qt-native logging macros to replace plog
// These provide a similar streaming interface as plog

class LogMessage {
public:
    LogMessage(QtMsgType type, const char* file, int line)
        : m_type(type), m_file(file), m_line(line) {}
    
    ~LogMessage() {
        QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
        QString level;
        switch (m_type) {
            case QtDebugMsg:    level = "DEBUG"; break;
            case QtInfoMsg:     level = "INFO "; break;
            case QtWarningMsg:  level = "WARN "; break;
            case QtCriticalMsg: level = "ERROR"; break;
            case QtFatalMsg:    level = "FATAL"; break;
        }
        
        QTextStream(stderr) << timestamp << " " << level << "  " << m_stream << "\n";
    }
    
    template<typename T>
    LogMessage& operator<<(const T& value) {
        QTextStream ts(&m_stream);
        ts << value;
        return *this;
    }
    
    // Overload for QString
    LogMessage& operator<<(const QString& value) {
        m_stream += value;
        return *this;
    }
    
    // Overload for std::string
    LogMessage& operator<<(const std::string& value) {
        m_stream += QString::fromStdString(value);
        return *this;
    }
    
    // Overload for const char*
    LogMessage& operator<<(const char* value) {
        m_stream += QString::fromUtf8(value);
        return *this;
    }

private:
    QtMsgType m_type;
    const char* m_file;
    int m_line;
    QString m_stream;
};

#define LOGD LogMessage(QtDebugMsg, __FILE__, __LINE__)
#define LOGI LogMessage(QtInfoMsg, __FILE__, __LINE__)
#define LOGW LogMessage(QtWarningMsg, __FILE__, __LINE__)
#define LOGE LogMessage(QtCriticalMsg, __FILE__, __LINE__)
#define LOGF LogMessage(QtFatalMsg, __FILE__, __LINE__)

#endif // LOGGING_H

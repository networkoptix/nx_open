#include <QtGui/QGuiApplication>

class QtLocalPeer;

class QtSingleGuiApplication: public QGuiApplication
{
    Q_OBJECT

public:
    QtSingleGuiApplication(int& argc, char** argv);
    QtSingleGuiApplication(const QString& id, int& argc, char** argv);

    bool isRunning();
    QString id() const;

public:
    bool sendMessage(const QString& message, int timeout = 5000);

signals:
    void messageReceived(const QString& message);

private:
    QtLocalPeer* m_peer = nullptr;
};

#ifndef EMAIL_H
#define EMAIL_H

#include <QString>
#include <QStringList>

class QnEmail {
public:
    enum ConnectionType {
        Unsecure,
        Ssl,
        Tls,
        ConnectionTypeCount
    };

    struct SmtpServerPreset {
        SmtpServerPreset() {}
        SmtpServerPreset(const QString &server, ConnectionType connectionType = Tls, int port = 0):
            server(server), connectionType(connectionType), port(port) {}

        bool isNull() const {
            return server.isEmpty();
        }

        QString server;
        ConnectionType connectionType;
        int port;
    };


    QnEmail(const QString &email);

    static bool isValid(const QString &email);
    static int defaultPort(ConnectionType connectionType);

    bool isValid() const;
    SmtpServerPreset smtpServer() const;
    QString domain() const;
private:
    QString m_email;
};


#endif // EMAIL_H

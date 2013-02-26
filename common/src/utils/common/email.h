#ifndef EMAIL_H
#define EMAIL_H

#include <QString>
#include <QStringList>

#include <api/model/kvpair.h>

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

    struct Settings {
        Settings();
        Settings(const QnKvPairList &values);

        bool isNull() const {
            return server.isEmpty();
        }

        QnKvPairList serialized() const;

        QString server;
        QString user;
        QString password;
        ConnectionType connectionType;
        int port;
        int timeout;

        //TODO: #GDM think where else we can store it
        /** Flag that we are using simple view */
        bool simple;
    };


    QnEmail(const QString &email);

    static bool isValid(const QString &email);
    static int defaultPort(ConnectionType connectionType);

    bool isValid() const;
    SmtpServerPreset smtpServer() const;
    Settings settings() const;
    QString domain() const;
private:
    QString m_email;
};


#endif // EMAIL_H

#ifndef EMAIL_H
#define EMAIL_H

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QMetaType>

#include <api/model/kvpair.h>

#include <utils/serialization/json.h>

class QnEmail {
    Q_GADGET
    Q_ENUMS(ConnectionType)

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
        ~Settings();

        bool isNull() const {
            return server.isEmpty();
        }

        QnKvPairList serialized() const;

        QString server;
        QString user;
        QString password;
        QString signature;
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

    /**
     * @brief smtpServer        This function should be called only from GUI thread
     *                          because it may call initSmtpPresets().
     * @return                  Corresponding smtp server preset if exists.
     */
    SmtpServerPreset smtpServer() const;

    Settings settings() const;
    QString domain() const;

private:
    /**
     * @brief initSmtpPresets   This function should be called only from GUI thread
     *                          because sync checks are not implemented.
     */
    void initSmtpPresets() const;

    QString m_email;
};

inline void serialize(const QnEmail::ConnectionType &value, QJsonValue *target) {
    QJson::serialize(static_cast<int>(value), target);
}

inline bool deserialize(const QJsonValue &value, QnEmail::ConnectionType *target) {
    int tmp;
    if(!QJson::deserialize(value, &tmp))
        return false;
    
    *target = static_cast<QnEmail::ConnectionType>(tmp);
    return true;
}

QN_DECLARE_FUNCTIONS(QnEmail::SmtpServerPreset, (json))

#endif // EMAIL_H

#ifndef EMAIL_H
#define EMAIL_H

#include <QString>
#include <QStringList>
#include <QtCore/QMetaType>

#include <api/model/kvpair.h>

#include <utils/common/json.h>

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

inline void serialize(const QnEmail::ConnectionType &value, QVariant *target) {
    *target = (int)value;
}

inline bool deserialize(const QVariant &value, QnEmail::ConnectionType *target) {
    *target = (QnEmail::ConnectionType)value.toInt();
    return true;
}

Q_DECLARE_METATYPE(QnEmail::SmtpServerPreset)
QN_DEFINE_STRUCT_SERIALIZATION_FUNCTIONS(QnEmail::SmtpServerPreset, (server)(connectionType)(port), inline)


#endif // EMAIL_H

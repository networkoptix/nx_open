#ifndef QN_EMAIL_H
#define QN_EMAIL_H

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QMetaType>

#include <api/model/kvpair.h>

#include <utils/common/model_functions_fwd.h>

class QnEmail {
    Q_GADGET
    Q_ENUMS(ConnectionType)

public:
    // TODO: #Elric #EC2 move out, rename?
    enum ConnectionType {
        Unsecure = 0,
        Ssl = 1,
        Tls = 2,
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

        bool isNull() const;
        bool isValid() const;

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

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(QnEmail::ConnectionType)
QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((QnEmail::SmtpServerPreset)(QnEmail::ConnectionType), (lexical))

#endif // QN_EMAIL_H


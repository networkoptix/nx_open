#ifndef QN_EMAIL_H
#define QN_EMAIL_H

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QMetaType>

#include <api/model/kvpair.h>

#include <boost/operators.hpp>

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
        SmtpServerPreset();
        SmtpServerPreset(const QString &server, ConnectionType connectionType = Tls, int port = 0);

        bool isNull() const {
            return server.isEmpty();
        }

        QString server;
        ConnectionType connectionType;
        int port;
    };

    struct Settings: public boost::equality_comparable1<Settings> {
        Settings();

        bool isNull() const;
        bool isValid() const;

        QString server;
        QString user;
        QString password;
        QString signature;
        QString supportEmail;
        ConnectionType connectionType;
        int port;
        int timeout;

        //TODO: #GDM #Common think where else we can store it
        /** Flag that we are using simple view */
        bool simple;

        friend bool operator==(const Settings &l, const Settings &r);
    };


    QnEmail(const QString &email);

    static bool isValid(const QString &email);
    static int defaultPort(ConnectionType connectionType);
    static int defaultTimeoutSec();

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
QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((QnEmail::SmtpServerPreset)(QnEmail::ConnectionType), (metatype)(lexical))

#endif // QN_EMAIL_H


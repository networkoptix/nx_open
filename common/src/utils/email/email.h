#pragma once

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QMetaType>

#include <nx/fusion/model_functions_fwd.h>
#include "email_fwd.h"


enum class SmtpReplyCode
{
    NoReply = 0,
    NonStandardSuccess = 200,
    SystemStatus = 211,
    Help = 214,
    ServiceReady = 220,
    ServiceClosedChannel = 221,
    AuthSuccessful = 235,
    MailActionOK = 250,
    UserNotLocal = 251,
    CannotVerifyUser = 252,
    ServerChallenge = 334,
    StartMailInput = 354,
    ServiceNotAvailable = 421,
    MailboxTemporaryUnavailable = 450,
    ProcessingError = 451,
    InsufficientStorage = 452,
    CommandSyntaxError = 500,
    ParameterSyntaxError = 501,
    CommandNotImplemented = 502,
    BadCommandSequence = 503,
    ParameterNotImplemented = 504,
    MailIsNotAccepted = 521,
    AccessDenied = 530,
    MailboxUnavailable = 550,
    UserNotLocalError = 551,
    ActionAborted = 552,
    InvalidMailboxName = 553,
    TransactionFailed = 554,
};
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(SmtpReplyCode)

enum class SmtpError
{
    Success,
    ConnectionTimeoutError,
    ResponseTimeoutError,
    SendDataTimeoutError,
    AuthenticationFailedError,
    ServerError,    // 4xx SMTP error
    ClientError     // 5xx SMTP error
};
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(SmtpError)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((SmtpReplyCode)(SmtpError), (metatype)(numeric))

struct SmtpOperationResult
{
    SmtpError error = SmtpError::Success;
    SmtpReplyCode lastCode = SmtpReplyCode::NoReply;

    SmtpOperationResult() {}
    SmtpOperationResult(SmtpError error):
        error(error)
    {
    }
    SmtpOperationResult(SmtpError error, SmtpReplyCode lastCode):
        error(error), lastCode(lastCode)
    {
    }

    explicit operator bool() const
    {
        return error == SmtpError::Success;
    }

    /** Note: untranslatable */
    QString toString() const;
};

#define SmtpOperationResult_Fields (error)(lastCode)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((SmtpOperationResult), (metatype)(lexical)(json))

// TODO: #GDM move other methods to this namespace, then move module to nx/email (?) lib
namespace nx {
namespace email {

/**
 *  Check is string looks like correct email address.
 *  Supports tags by RFC5233 ( user+tag@domain.com ).
 *  Supports custom names ( Vasya Pupkin <vasya@pupkin.com> )
 *  Domain length is limited to 255 symbols.
 */
bool isValidAddress(const QString& address);

} // namespace email
} // namespace nx

struct QnEmailSmtpServerPreset {
    QnEmailSmtpServerPreset();
    QnEmailSmtpServerPreset(const QString &server, QnEmail::ConnectionType connectionType = QnEmail::Tls, int port = 0);

    bool isNull() const {
        return server.isEmpty();
    }

    QString server;
    QnEmail::ConnectionType connectionType;
    int port;
};
#define QnEmailSmtpServerPreset_Fields (server)(connectionType)(port)

struct QnEmailSettings {
    QnEmailSettings();

    QString email;                      /**< Sender email. Used as MAIL FROM. */
    QString server;                     /**< Target smtp server. */
    QString user;                       /**< Username for smtp authorization. */
    QString password;                   /**< Password for smtp authorization. */
    QString signature;                  /**< Signature text. Used in the email text. */
    QString supportEmail;               /**< Support link. Named as email to maintain compatibility. */
    QnEmail::ConnectionType connectionType;      /**< Connection protocol (TLS/SSL/Unsecure). */
    int port;                           /**< Smtp server port. */
    int timeout;                        /**< Connection timeout. */

    // TODO: #GDM #Common think where else we can store it
    bool simple;                        /**< Flag that we are using simple view. */

    bool isValid() const;

    bool equals(const QnEmailSettings &other, bool compareView = false) const;

    static int defaultPort(QnEmail::ConnectionType connectionType);
    static int defaultTimeoutSec();
};
#define QnEmailSettings_Fields (email)(server)(user)(password)(signature)(supportEmail)(connectionType)(port)(timeout)(simple)

/**
 * Generic email address class.
 * Supports user names and tagged addressing by RFC5233.
 * Examples:
 * * Vasya Pupkin <vasya@pupkin.com>
 * * user+tag@domain.com
 */
class QnEmailAddress
{
public:
    explicit QnEmailAddress(const QString &email);

    bool isValid() const;

    /**
    * @brief smtpServer        This function should be called only from GUI thread
    *                          because it may call initSmtpPresets().
    * @return                  Corresponding smtp server preset if exists.
    */
    QnEmailSmtpServerPreset smtpServer() const;

    QnEmailSettings settings() const;

    /** Username. For tagged addresses returns base part. */
    QString user() const;

    /** Smtp domain */
    QString domain() const;

    /** Email value. */
    QString value() const;

    /** User's full name (if provided) */
    QString fullName() const;

    bool operator==(const QnEmailAddress& other) const;

private:
    /**
    * @brief initSmtpPresets   This function should be called only from GUI thread
    *                          because sync checks are not implemented.
    */
    void initSmtpPresets() const;

    QString m_email;
    QString m_fullName;
};

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((QnEmailSmtpServerPreset)(QnEmailSettings), (metatype)(lexical)(json))

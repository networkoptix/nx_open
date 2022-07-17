// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QMetaType>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/enum_instrument.h>

#include "email_fwd.h"

namespace nx::email {

NX_REFLECTION_ENUM_CLASS(SmtpError,
    success,
    connectionTimeout,
    responseTimeout,
    sendDataTimeout,
    authenticationFailed,
    serverFailure,    // 4xx SMTP error
    clientFailure     // 5xx SMTP error
)

} // namespace nx::email

struct NX_VMS_COMMON_API SmtpOperationResult
{
    nx::email::SmtpError error = nx::email::SmtpError::success;
    int lastCode = SmtpReplyCode::NoReply;

    SmtpOperationResult() {}
    SmtpOperationResult(nx::email::SmtpError error):
        error(error)
    {
    }
    SmtpOperationResult(nx::email::SmtpError error, SmtpReplyCode lastCode):
        error(error), lastCode(lastCode)
    {
    }

    explicit operator bool() const
    {
        return error == nx::email::SmtpError::success;
    }

    /** Note: untranslatable */
    QString toString() const;
};

#define SmtpOperationResult_Fields (error)(lastCode)

QN_FUSION_DECLARE_FUNCTIONS(SmtpOperationResult, (metatype)(lexical)(json), NX_VMS_COMMON_API)

// TODO: #sivanov Move other methods to this namespace, then move module to nx/email (?) lib.
namespace nx {
namespace email {

/**
 *  Check is string looks like correct email address.
 *  Supports tags by RFC5233 ( user+tag@domain.com ).
 *  Supports custom names ( Vasya Pupkin <vasya@pupkin.com> )
 *  Domain length is limited to 255 symbols.
 */
NX_VMS_COMMON_API bool isValidAddress(const QString& address);

} // namespace email
} // namespace nx

struct QnEmailSmtpServerPreset {
    QnEmailSmtpServerPreset();
    QnEmailSmtpServerPreset(
        const QString& server,
        QnEmail::ConnectionType connectionType = QnEmail::ConnectionType::tls,
        int port = 0);

    bool isNull() const {
        return server.isEmpty();
    }

    QString server;
    QnEmail::ConnectionType connectionType;
    int port;
};
#define QnEmailSmtpServerPreset_Fields (server)(connectionType)(port)

struct NX_VMS_COMMON_API QnEmailSettings
{
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

    // TODO: #sivanov Think where else we can store it.
    bool simple;                        /**< Flag that we are using simple view. */

    QString name;

    bool isEmpty() const;
    bool isValid() const;

    bool equals(const QnEmailSettings& other, bool compareView = false,
        bool comparePassword = true) const;

    static int defaultPort(QnEmail::ConnectionType connectionType);
    static int defaultTimeoutSec();
};
#define QnEmailSettings_Fields \
    (email)(server)(user)(password)(signature)(supportEmail)(connectionType)(port)(timeout)(simple)(name)

/**
 * Generic email address class.
 * Supports user names and tagged addressing by RFC5233.
 * Examples:
 * * Vasya Pupkin <vasya@pupkin.com>
 * * user+tag@domain.com
 */
class NX_VMS_COMMON_API QnEmailAddress
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

QN_FUSION_DECLARE_FUNCTIONS(QnEmailSmtpServerPreset, (metatype)(lexical)(json), NX_VMS_COMMON_API)
QN_FUSION_DECLARE_FUNCTIONS(QnEmailSettings, (metatype)(lexical)(json), NX_VMS_COMMON_API)

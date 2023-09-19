// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "email.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <QtCore/QFile>
#include <QtCore/QHash>
#include <QtCore/QRegularExpression>
#include <QtCore/QThread>

#include <nx/fusion/model_functions.h>
#include <nx/utils/url.h>

namespace {

typedef QHash<QString, QnEmailSmtpServerPreset> QnSmtpPresets;

/* Top-level domains can already be up to 30 symbols length for now, so do not limiting them. */
static const QString kEmailPattern =
    R"(^[-!#$%&'*+/=?^_`{}|~0-9a-zA-Z]+(\.[-!#$%&'*+/=?^_`{}|~0-9a-zA-Z]+)*@(?:[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?\.)+[a-zA-Z]{2,30}\.?$)";

/* RFC5233 (sub-addressing, also known as plus addressing or tagged addressing). */
static const QString kFullNamePattern = "^(?<fullname>.*)<(?<email>.+)>$";

static constexpr int kTlsPort = 587;
static constexpr int kSslPort = 465;
static constexpr int kUnsecurePort = 25;
static constexpr int kAutoPort = 0;

static constexpr int kDefaultSmtpTimeout = 300; //< Seconds, 5 min.

} // namespace

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(SmtpOperationResult, (json), SmtpOperationResult_Fields)

QString SmtpOperationResult::toString() const
{
    return QString::fromUtf8(QJson::serialized(*this));
}

bool nx::email::isValidAddress(const QString& address)
{
    return QnEmailAddress(address).isValid();
}

static QnSmtpPresets smtpServerPresetPresets;
static bool smtpInitialized = false;

QnEmailSmtpServerPreset::QnEmailSmtpServerPreset():
    connectionType(QnEmail::ConnectionType::insecure),
    port(kAutoPort)
{}

QnEmailSmtpServerPreset::QnEmailSmtpServerPreset(
    const QString& server,
    QnEmail::ConnectionType connectionType,
    int port)
    :
    server(server),
    connectionType(connectionType),
    port(port)
{}

QnEmailSettings::QnEmailSettings(const nx::vms::api::EmailSettings& settings):
    nx::vms::api::EmailSettings(settings){}

int QnEmailSettings::defaultTimeoutSec()
{
    return kDefaultSmtpTimeout;
}

int QnEmailSettings::defaultPort(QnEmail::ConnectionType connectionType)
{
    switch (connectionType)
    {
        case QnEmail::ConnectionType::ssl:
            return kSslPort;
        case QnEmail::ConnectionType::tls:
            return kTlsPort;
        default:
            return kUnsecurePort;
    }
}

bool QnEmailSettings::isValid() const
{
    return QnEmailAddress(email).isValid() && nx::utils::Url::fromUserInput(server).isValid();
}

bool QnEmailSettings::isValid(const QString& email, const QString& server)
{
    return QnEmailAddress(email).isValid() && nx::utils::Url::fromUserInput(server).isValid();
}

QnEmailAddress::QnEmailAddress(const QString& email):
    m_email(email.trimmed())
{
    QRegularExpression re(kFullNamePattern);
    NX_ASSERT(re.isValid());

    const auto match = re.match(m_email);
    if (match.hasMatch())
    {
        m_fullName = match.captured("fullname").trimmed();
        m_email = match.captured("email").trimmed();
    }
    m_email = m_email.toLower();
}

bool QnEmailAddress::isValid() const
{
    QRegularExpression re(kEmailPattern);
    NX_ASSERT(re.isValid());
    const auto match = re.match(m_email);
    return match.hasMatch();
}

QString QnEmailAddress::user() const
{
    /* Support for tagged addressing: username+tag@domain.com */

    const auto idx = m_email.indexOf('@');
    const auto idxSuffix = m_email.indexOf('+');
    if (idx >= 0)
    {
        if (idxSuffix >= 0)
            return m_email.left(std::min(idx, idxSuffix));
        return m_email.left(idx);
    }

    return QString();
}

QString QnEmailAddress::domain() const
{
    const auto idx = m_email.indexOf('@');
    return m_email.mid(idx + 1);
}

QString QnEmailAddress::value() const
{
    return m_email;
}

QString QnEmailAddress::fullName() const
{
    return m_fullName;
}

bool QnEmailAddress::operator==(const QnEmailAddress& other) const
{
    return m_email == other.m_email
        && m_fullName == other.m_fullName;
}

QnEmailSmtpServerPreset QnEmailAddress::smtpServer() const
{
    if (!isValid())
        return QnEmailSmtpServerPreset();

    if (!smtpInitialized)
        initSmtpPresets();

    QString key = domain();
    if (smtpServerPresetPresets.contains(key))
        return smtpServerPresetPresets[key];
    return QnEmailSmtpServerPreset();
}

QnEmailSettings QnEmailAddress::settings() const
{
    QnEmailSettings result;
    QnEmailSmtpServerPreset preset = smtpServer();

    result.server = preset.server;
    result.port = preset.port;
    result.connectionType = preset.connectionType;

    return result;
}

void QnEmailAddress::initSmtpPresets() const
{
    NX_ASSERT(qApp && qApp->thread() == QThread::currentThread());

    QFile file(QLatin1String(":/smtp.json"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    if (!QJson::deserialize(file.readAll(), &smtpServerPresetPresets))
        qWarning() << "Smtp Presets file could not be parsed!";
    smtpInitialized = true;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    QnEmailSmtpServerPreset, (json), QnEmailSmtpServerPreset_Fields)

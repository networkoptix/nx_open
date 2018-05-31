#include "email.h"

#include <QtCore/QDebug>
#include <QtCore/QRegExp>
#include <QHash>
#include <QtCore/QFile>
#include <QtCore/QThread>

#include <QtCore/QCoreApplication>

#include <nx/fusion/model_functions.h>

namespace {

typedef QHash<QString, QnEmailSmtpServerPreset> QnSmtpPresets;

/* Top-level domains can already be up to 30 symbols length for now, so do not limiting them. */
const QLatin1String emailPattern("\\b[a-z0-9._%+-]+@[a-z0-9.-]+\\.[a-z]{2,255}\\b");

/* RFC5233 (sub-addressing, also known as plus addressing or tagged addressing). */
const QLatin1String fullNamePattern("(.*)<(.+)>");

const int tlsPort = 587;
const int sslPort = 465;
const int unsecurePort = 25;

const int defaultSmtpTimeout = 300; //seconds, 5 min

}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((SmtpOperationResult), (json), _Fields)

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

QnEmailSmtpServerPreset::QnEmailSmtpServerPreset() :
    connectionType(QnEmail::ConnectionType::unsecure),
    port(0)
{}

QnEmailSmtpServerPreset::QnEmailSmtpServerPreset(const QString &server, QnEmail::ConnectionType connectionType /* = Tls*/, int port /* = 0*/) :
    server(server),
    connectionType(connectionType),
    port(port)
{}

QnEmailSettings::QnEmailSettings() :
    connectionType(QnEmail::ConnectionType::unsecure),
    port(0),
    timeout(defaultSmtpTimeout),
    simple(true)
{}

int QnEmailSettings::defaultTimeoutSec()
{
    return defaultSmtpTimeout;
}

int QnEmailSettings::defaultPort(QnEmail::ConnectionType connectionType)
{
    switch (connectionType)
    {
        case QnEmail::ConnectionType::ssl:
            return sslPort;
        case QnEmail::ConnectionType::tls:
            return tlsPort;
        default:
            return unsecurePort;
    }
}

bool QnEmailSettings::isValid() const
{
    return !email.isEmpty() && !server.isEmpty();
}

bool QnEmailSettings::equals(const QnEmailSettings &other, bool compareView /* = false*/) const
{
    if (email != other.email)                   return false;
    if (server != other.server)                 return false;
    if (user != other.user)                     return false;
    if (password != other.password)             return false;
    if (signature != other.signature)           return false;
    if (supportEmail != other.supportEmail)     return false;
    if (connectionType != other.connectionType) return false;
    if (port != other.port)                     return false;
    if (timeout != other.timeout)               return false;

    return !compareView || (simple == other.simple);
}

QnEmailAddress::QnEmailAddress(const QString &email):
    m_email(email.trimmed())
{
    QRegExp rx(fullNamePattern);
    if (rx.exactMatch(m_email))
    {
        auto parts = rx.capturedTexts();
        NX_ASSERT(parts.size() == 3);
        if (parts.size() == 3)
        {
            m_fullName = parts[1].trimmed();
            m_email = parts[2].trimmed();
        }
    }

    m_email = m_email.toLower();
}

bool QnEmailAddress::isValid() const
{
    QRegExp rx(emailPattern);
    return rx.exactMatch(m_email);
}

QString QnEmailAddress::user() const
{
    /* Support for tagged addressing: username+tag@domain.com */

    int idx = m_email.indexOf(L'@');
    int idxSuffix = m_email.indexOf(L'+');
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
    int idx = m_email.indexOf(L'@');
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

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (QnEmailSmtpServerPreset)(QnEmailSettings), (json)(eq), _Fields)


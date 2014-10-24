#include "email.h"

#include <QtCore/QDebug>
#include <QtCore/QRegExp>
#include <QHash>
#include <QtCore/QFile>
#include <QtCore/QThread>

#include <QtCore/QCoreApplication>

#include <utils/common/model_functions.h>

QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(QnEmail, ConnectionType);

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((QnEmailSmtpServerPreset)(QnEmailSettings), (json)(eq), _Fields)

namespace {
    typedef QHash<QString, QnEmailSmtpServerPreset> QnSmtpPresets;

    const QLatin1String emailPattern("\\b[A-Z0-9._%+-]+@[A-Z0-9.-]+\\.[A-Z]{2,4}\\b");

    const int tlsPort = 587;
    const int sslPort = 465;
    const int unsecurePort = 25;

    const int defaultSmtpTimeout = 300; //seconds, 5 min
}

static QnSmtpPresets smtpServerPresetPresets;
static bool smtpInitialized = false;


QnEmailSmtpServerPreset::QnEmailSmtpServerPreset():
    connectionType(QnEmail::Unsecure),
    port(0)
{}

QnEmailSmtpServerPreset::QnEmailSmtpServerPreset(const QString &server, QnEmail::ConnectionType connectionType /*= Tls*/, int port /*= 0*/):
    server(server),
    connectionType(connectionType), 
    port(port) 
{}

QnEmailSettings::QnEmailSettings():
    connectionType(QnEmail::Unsecure),
    port(0),
    timeout(defaultSmtpTimeout),
    simple(true)
{}

int QnEmailSettings::defaultTimeoutSec() {
    return defaultSmtpTimeout;
}

int QnEmailSettings::defaultPort(QnEmail::ConnectionType connectionType) {
    switch (connectionType) {
    case QnEmail::Ssl: return sslPort;
    case QnEmail::Tls: return tlsPort;
    default:
        return unsecurePort;
    }
}


bool QnEmailSettings::isNull() const {
    return server.isEmpty();
}

bool QnEmailSettings::isValid() const {
    return !server.isEmpty() && !user.isEmpty() && !password.isEmpty();
}


QnEmailAddress::QnEmailAddress(const QString &email):
    m_email(email.trimmed().toLower())
{
}

bool QnEmailAddress::isValid() const {
    return isValid(m_email);
}

bool QnEmailAddress::isValid(const QString &email) {
    QRegExp rx(emailPattern);
    return rx.exactMatch(email.trimmed().toUpper());
}

QString QnEmailAddress::user() const {
    int idx = m_email.indexOf(QLatin1Char('@'));
    return m_email.left(idx).trimmed();
}

QString QnEmailAddress::domain() const {
    int idx = m_email.indexOf(QLatin1Char('@'));
    return m_email.mid(idx + 1).trimmed();
}

QnEmailSmtpServerPreset QnEmailAddress::smtpServer() const {
    if (!isValid())
        return QnEmailSmtpServerPreset();

    if (!smtpInitialized)
        initSmtpPresets();

    QString key = domain();
    if (smtpServerPresetPresets.contains(key))
        return smtpServerPresetPresets[key];
    return QnEmailSmtpServerPreset();
}

QnEmailSettings QnEmailAddress::settings() const {
    QnEmailSettings result;
    QnEmailSmtpServerPreset preset = smtpServer();

    result.server = preset.server;
    result.port = preset.port;
    result.connectionType = preset.connectionType;

    return result;
}

void QnEmailAddress::initSmtpPresets() const {
    Q_ASSERT(qApp && qApp->thread() == QThread::currentThread());

    QFile file(QLatin1String(":/smtp.json"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    if(!QJson::deserialize(file.readAll(), &smtpServerPresetPresets))
        qWarning() << "Smtp Presets file could not be parsed!";
    smtpInitialized = true;
}

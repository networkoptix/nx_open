#include "email.h"

#include <QtCore/QDebug>
#include <QtCore/QRegExp>
#include <QHash>
#include <QtCore/QFile>
#include <QtCore/QThread>

#include <QtCore/QCoreApplication>

#include <utils/fusion/fusion_adaptor.h>
#include <utils/common/model_functions.h>


QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(QnEmail, ConnectionType);

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnEmail::SmtpServerPreset, (json), (server)(connectionType)(port))

void serialize(const QnEmail::ConnectionType &value, QJsonValue *target) {
    QJson::serialize(static_cast<int>(value), target);
}

bool deserialize(const QJsonValue &value, QnEmail::ConnectionType *target) {
    int tmp;
    if(!QJson::deserialize(value, &tmp))
        return false;

    *target = static_cast<QnEmail::ConnectionType>(tmp);
    return true;
}


namespace {
    typedef QHash<QString, QnEmail::SmtpServerPreset> QnSmtpPresets;

    const QLatin1String emailPattern("\\b[A-Z0-9._%+-]+@[A-Z0-9.-]+\\.[A-Z]{2,4}\\b");

    const int tlsPort = 587;
    const int sslPort = 465;
    const int unsecurePort = 25;

    const int defaultSmtpTimeout = 300; //seconds, 5 min

    static QnSmtpPresets smtpServerPresetPresets;
    static bool smtpInitialized = false;
}

QnEmail::SmtpServerPreset::SmtpServerPreset():
    connectionType(QnEmail::Unsecure),
    port(0)
{}

QnEmail::SmtpServerPreset::SmtpServerPreset(const QString &server, ConnectionType connectionType /*= Tls*/, int port /*= 0*/):
    server(server),
    connectionType(connectionType), 
    port(port) 
{}

QnEmail::QnEmail(const QString &email):
    m_email(email.trimmed().toLower())
{
}

bool QnEmail::isValid() const {
    return isValid(m_email);
}

bool QnEmail::isValid(const QString &email) {
    QRegExp rx(emailPattern);
    return rx.exactMatch(email.trimmed().toUpper());
}

int QnEmail::defaultPort(QnEmail::ConnectionType connectionType) {
    switch (connectionType) {
    case Ssl: return sslPort;
    case Tls: return tlsPort;
    default:
        return unsecurePort;
    }
}

QString QnEmail::domain() const {
    int idx = m_email.indexOf(QLatin1Char('@'));
    return m_email.mid(idx + 1).trimmed();
}

QnEmail::SmtpServerPreset QnEmail::smtpServer() const {
    if (!isValid())
        return SmtpServerPreset();

    if (!smtpInitialized)
        initSmtpPresets();

    QString key = domain();
    if (smtpServerPresetPresets.contains(key))
        return smtpServerPresetPresets[key];
    return SmtpServerPreset();
}

QnEmail::Settings QnEmail::settings() const {
    Settings result;
    SmtpServerPreset preset = smtpServer();

    result.server = preset.server;
    result.port = preset.port;
    result.connectionType = preset.connectionType;

    return result;
}

QnEmail::Settings::Settings():
    connectionType(QnEmail::Unsecure),
    port(0),
    timeout(defaultSmtpTimeout),
    simple(true)
{}

bool QnEmail::Settings::isNull() const {
    return server.isEmpty();
}

bool QnEmail::Settings::isValid() const {
    return !server.isEmpty() && !user.isEmpty() && !password.isEmpty();
}

void QnEmail::initSmtpPresets() const {
    Q_ASSERT(qApp && qApp->thread() == QThread::currentThread());

    QFile file(QLatin1String(":/smtp.json"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    if(!QJson::deserialize(file.readAll(), &smtpServerPresetPresets))
        qWarning() << "Smtp Presets file could not be parsed!";
    smtpInitialized = true;
}

int QnEmail::defaultTimeoutSec() {
    return defaultSmtpTimeout;
}

bool operator==(const QnEmail::Settings &l, const QnEmail::Settings &r) {
    return 
        l.server == r.server &&
        l.user == r.user &&
        l.password == r.password &&
        l.signature == r.signature &&
        l.supportEmail == r.supportEmail &&
        l.connectionType == r.connectionType &&
        l.port == r.port &&
        l.timeout == r.timeout &&
        l.simple == r.simple;
}

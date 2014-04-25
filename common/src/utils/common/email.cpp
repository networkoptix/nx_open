#include "email.h"

#include <QtCore/QDebug>
#include <QtCore/QRegExp>
#include <QHash>
#include <QtCore/QFile>
#include <QtCore/QThread>

#include <QtCore/QCoreApplication>

#include "enum_name_mapper.h"

QN_DEFINE_METAOBJECT_ENUM_NAME_MAPPING(QnEmail, ConnectionType);
QN_DEFINE_ENUM_MAPPED_LEXICAL_JSON_SERIALIZATION_FUNCTIONS(QnEmail::ConnectionType);
QN_DEFINE_STRUCT_JSON_SERIALIZATION_FUNCTIONS(QnEmail::SmtpServerPreset, (server)(connectionType)(port))

namespace {
    typedef QHash<QString, QnEmail::SmtpServerPreset> QnSmtpPresets;

    const QLatin1String emailPattern("\\b[A-Z0-9._%+-]+@[A-Z0-9.-]+\\.[A-Z]{2,4}\\b");

    static const int tlsPort = 587;
    static const int sslPort = 465;
    static const int unsecurePort = 25;

    const QLatin1String nameFrom("EMAIL_FROM");
    const QLatin1String nameHost("EMAIL_HOST");
    const QLatin1String namePort("EMAIL_PORT");
    const QLatin1String nameUser("EMAIL_HOST_USER");
    const QLatin1String namePassword("EMAIL_HOST_PASSWORD");
    const QLatin1String nameTls("EMAIL_USE_TLS");
    const QLatin1String nameSsl("EMAIL_USE_SSL");
    const QLatin1String nameSimple("EMAIL_SIMPLE");
    const QLatin1String nameTimeout("EMAIL_TIMEOUT");
    const QLatin1String nameSignature("systemName");
    const int TIMEOUT = 20; //seconds

    static QnSmtpPresets smtpServerPresetPresets;
    static bool smtpInitialized = false;
}

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
    timeout(TIMEOUT),
    simple(true)
{}

QnEmail::Settings::Settings(const QnKvPairList &values):
    port(0),
    timeout(TIMEOUT),
    simple(true)
{
    bool useTls = false;
    bool useSsl = false;

    foreach (const QnKvPair &setting, values) {
        if (setting.name() == nameHost) {
            server = setting.value();
        } else if (setting.name() == namePort) {
            port = setting.value().toInt();
        } else if (setting.name() == nameUser) {
            user = setting.value();
        } else if (setting.name() == namePassword) {
            password = setting.value();
        } else if (setting.name() == nameTls) {
            useTls = setting.value() == QLatin1String("True");
        } else if (setting.name() == nameSsl) {
            useSsl = setting.value() == QLatin1String("True");
        } else if (setting.name() == nameSimple) {
            simple = setting.value() == QLatin1String("True");
        } else if (setting.name() == nameSignature) {
            signature = setting.value();
        }
    }

    connectionType = useTls
            ? QnEmail::Tls
            : useSsl
              ? QnEmail::Ssl
              : QnEmail::Unsecure;
    if (port == defaultPort(connectionType))
        port = 0;
}

QnEmail::Settings::~Settings() {
}

bool QnEmail::Settings::isNull() const {
    return server.isEmpty();
}

bool QnEmail::Settings::isValid() const {
    return !server.isEmpty() && !user.isEmpty() && !password.isEmpty();
}

QnKvPairList QnEmail::Settings::serialized() const {
    QnKvPairList result;

    bool useTls = connectionType == QnEmail::Tls;
    bool useSsl = connectionType == QnEmail::Ssl;
    bool valid = isValid();

    result
    << QnKvPair(nameHost, server)
    << QnKvPair(namePort, port == 0 ? defaultPort(connectionType) : port)
    << QnKvPair(nameUser, user)
    << QnKvPair(nameFrom, user)
    << QnKvPair(namePassword, valid ? password : QString())
    << QnKvPair(nameTls, useTls)
    << QnKvPair(nameSsl, useSsl)
    << QnKvPair(nameSimple, simple)
    << QnKvPair(nameTimeout, TIMEOUT)
    << QnKvPair(nameSignature, signature);
    return result;
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


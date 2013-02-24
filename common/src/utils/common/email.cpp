#include "email.h"

#include <QRegExp>
#include <QHash>

namespace {
    const QLatin1String emailPattern("\\b[A-Z0-9._%+-]+@[A-Z0-9.-]+\\.[A-Z]{2,4}\\b");

    static const int tlsPort = 587;
    static const int sslPort = 465;
    static const int unsecurePort = 25;

    static QHash<QString, QnEmail::SmtpServerPreset> initSmtpPresets() {
        typedef QLatin1String _;
        typedef QnEmail::SmtpServerPreset server;

        QHash<QString, QnEmail::SmtpServerPreset> presets;
        presets.insert(_("gmail.com"),          server(_("smtp.gmail.com")));

        presets.insert(_("hotmail.co.uk"),      server(_("smtp.live.com")));
        presets.insert(_("hotmail.com"),        server(_("smtp.live.com")));

        presets.insert(_("dwcc.tv"),            server(_("mail.dwcc.tv")));

        presets.insert(_("yahoo.com"),          server(_("smtp.mail.yahoo.com"), QnEmail::Ssl));
        presets.insert(_("yahoo.co.uk"),        server(_("smtp.mail.yahoo.co.uk"), QnEmail::Ssl));
        presets.insert(_("yahoo.de"),           server(_("smtp.mail.yahoo.de"), QnEmail::Ssl));
        presets.insert(_("yahoo.com.au"),       server(_("smtp.mail.yahoo.com.au"), QnEmail::Ssl));

        presets.insert(_("o2.ie"),              server(_("smtp.o2.ie"), QnEmail::Unsecure));
        presets.insert(_("o2.co.uk"),           server(_("smtp.o2.co.uk"), QnEmail::Unsecure));

        presets.insert(_("inbox.com"),          server(_("my.inbox.com")));
/*
        presets.insert(_("outlook.com"),        SmtpServerPreset(_("")));
        presets.insert(_("msn"),                SmtpServerPreset(_("")));
        presets.insert(_("sympatico.ca"),       SmtpServerPreset(_("")));
        presets.insert(_("telus.net"),          SmtpServerPreset(_("")));
        presets.insert(_("shaw.ca"),            SmtpServerPreset(_("")));*/

        return presets;
    }

    static const QHash<QString, QnEmail::SmtpServerPreset> SmtpServerPresetPresets = initSmtpPresets();
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

    QString key = domain();
    if (SmtpServerPresetPresets.contains(key))
        return SmtpServerPresetPresets[key];
    return SmtpServerPreset(QLatin1String("smtp.") + key, Unsecure);
}

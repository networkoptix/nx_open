#include "email.h"

#include <QRegExp>
#include <QHash>

namespace {
    const QLatin1String emailPattern("\\b[A-Z0-9._%+-]+@[A-Z0-9.-]+\\.[A-Z]{2,4}\\b");

    typedef QLatin1String _;

    static QHash<QString, QnSmptServerPreset> initSmtpPresets() {
        QHash<QString, QnSmptServerPreset> presets;
        presets.insert(_("gmail.com"),          QnSmptServerPreset(_("smtp.gmail.com")));

        presets.insert(_("hotmail.co.uk"),      QnSmptServerPreset(_("smtp.live.com")));
        presets.insert(_("hotmail.com"),        QnSmptServerPreset(_("smtp.live.com")));

        presets.insert(_("dwcc.tv"),            QnSmptServerPreset(_("mail.dwcc.tv")));

        presets.insert(_("yahoo.com"),          QnSmptServerPreset(_("smtp.mail.yahoo.com"), true, 465));
        presets.insert(_("yahoo.co.uk"),        QnSmptServerPreset(_("smtp.mail.yahoo.co.uk"), true, 465));
        presets.insert(_("yahoo.de"),           QnSmptServerPreset(_("smtp.mail.yahoo.de"), true, 465));
        presets.insert(_("yahoo.com.au"),       QnSmptServerPreset(_("smtp.mail.yahoo.com.au"), true, 465));

        presets.insert(_("o2.ie"),              QnSmptServerPreset(_("smtp.o2.ie"), false));
        presets.insert(_("o2.co.uk"),           QnSmptServerPreset(_("smtp.o2.co.uk"), false));

        presets.insert(_("inbox.com"),          QnSmptServerPreset(_("my.inbox.com")));
/*
        presets.insert(_("outlook.com"),        QnSmptServerPreset(_("")));
        presets.insert(_("msn"),                QnSmptServerPreset(_("")));
        presets.insert(_("sympatico.ca"),       QnSmptServerPreset(_("")));
        presets.insert(_("telus.net"),          QnSmptServerPreset(_("")));
        presets.insert(_("shaw.ca"),            QnSmptServerPreset(_("")));*/

        return presets;
    }

    static const QHash<QString, QnSmptServerPreset> smtpServerPresets = initSmtpPresets();
}

QnSmptServerPreset::QnSmptServerPreset(const QString &server, const bool useTls, const int port) :
    server(server), useTls(useTls), port(port) {
    if (port != 0)
        return;
    if (useTls)
        this->port = QnEmail::securePort;
    else
        this->port = QnEmail::unsecurePort;
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

QString QnEmail::domain() const {
    int idx = m_email.indexOf(QLatin1Char('@'));
    return m_email.mid(idx + 1).trimmed();
}

QnSmptServerPreset QnEmail::smptServer() const {
    QString key = domain();
    if (smtpServerPresets.contains(key))
        return smtpServerPresets[key];
    return QnSmptServerPreset(QLatin1String("smtp.") + key, false);
}

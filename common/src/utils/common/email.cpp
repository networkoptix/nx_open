#include "email.h"

#include <QRegExp>
#include <QHash>

namespace {
    const QLatin1String emailPattern("\\b[A-Z0-9._%+-]+@[A-Z0-9.-]+\\.[A-Z]{2,4}\\b");

    static const int tlsPort = 587;
    static const int sslPort = 465;
    static const int unsecurePort = 25;

    //TODO: #GDM check authorization login: use domain or not
    static QHash<QString, QnEmail::SmtpServerPreset> initSmtpPresets() {
        typedef QLatin1String _;
        typedef QnEmail::SmtpServerPreset server;

        QHash<QString, QnEmail::SmtpServerPreset> presets;
        presets.insert(_("gmail.com"),          server(_("smtp.gmail.com")));

        presets.insert(_("hotmail.co.uk"),      server(_("smtp.live.com")));
        presets.insert(_("hotmail.com"),        server(_("smtp.live.com")));
        presets.insert(_("outlook.com"),        server(_("smtp.live.com")));

        presets.insert(_("dwcc.tv"),            server(_("mail.dwcc.tv")));

        presets.insert(_("yahoo.com"),          server(_("smtp.mail.yahoo.com"), QnEmail::Ssl));
        presets.insert(_("yahoo.co.uk"),        server(_("smtp.mail.yahoo.co.uk"), QnEmail::Ssl));
        presets.insert(_("yahoo.de"),           server(_("smtp.mail.yahoo.de"), QnEmail::Ssl));
        presets.insert(_("yahoo.com.au"),       server(_("smtp.mail.yahoo.com.au"), QnEmail::Ssl));

        presets.insert(_("att.yahoo.com"),       server(_("smtp.att.yahoo.com"), QnEmail::Ssl));

        presets.insert(_("o2.ie"),              server(_("smtp.o2.ie"), QnEmail::Unsecure));
        presets.insert(_("o2.co.uk"),           server(_("smtp.o2.co.uk"), QnEmail::Unsecure));
        presets.insert(_("o2online.de"),        server(_("mail.o2online.de"), QnEmail::Unsecure));

        presets.insert(_("inbox.com"),          server(_("my.inbox.com")));
        presets.insert(_("aol.com"),            server(_("smtp.aol.com")));

        presets.insert(_("sympatico.ca"),       server(_("smtphm.sympatico.ca"), QnEmail::Ssl, 25));
        presets.insert(_("bell.net"),           server(_("smtphm.sympatico.ca"), QnEmail::Ssl, 25));

        presets.insert(_("telus.net"),          server(_("smtp.telus.net"), QnEmail::Unsecure));

        presets.insert(_("shaw.ca"),            server(_("smtp.telus.net"), QnEmail::Unsecure));

        presets.insert(_("cogeco.ca"),          server(_("smtp.telus.net"), QnEmail::Unsecure));
        presets.insert(_("lycos.com"),          server(_("smtp.mail.lycos.com"), QnEmail::Unsecure));
        presets.insert(_("mail.com"),           server(_("smtp.mail.com"), QnEmail::Ssl));
        presets.insert(_("netscape.com"),       server(_("mail.netscape.ca")));
        presets.insert(_("rogers.com"),         server(_("smtp.broadband.rogers.com")));
        presets.insert(_("videotron.ca "),      server(_("smtp.telus.net"), QnEmail::Unsecure));
        presets.insert(_("ntlworld.com"),       server(_("smtp.ntlworld.com"), QnEmail::Ssl));
        presets.insert(_("btconnect.com"),      server(_("mail.btconnect.com"), QnEmail::Unsecure));


        presets.insert(_("1and1.com"),          server(_("smtp.1and1.com")));
        presets.insert(_("1und1.de"),           server(_("smtp.1und1.de")));
        presets.insert(_("comcast.net"),        server(_("smtp.comcast.net")));


        presets.insert(_("t-online.de"),        server(_("securesmtp.t-online.de"), QnEmail::Ssl));
        presets.insert(_("verizon.net"),        server(_("outgoing.verizon.net"), QnEmail::Ssl));
        presets.insert(_("yahoo.verizon.net"),  server(_("outgoing.yahoo.verizon.net"), QnEmail::Ssl));

        presets.insert(_("btopenworld.com"),    server(_("mail.btopenworld.com"), QnEmail::Unsecure));
        presets.insert(_("btinternet.com"),     server(_("mail.btinternet.com"), QnEmail::Unsecure));
        presets.insert(_("orange.net"),         server(_("smtp.orange.net"), QnEmail::Unsecure));
        presets.insert(_("orange.co.uk"),       server(_("smtp.orange.co.uk"), QnEmail::Unsecure));
        presets.insert(_("wanadoo.co.uk"),      server(_("smtp.wanadoo.co.uk"), QnEmail::Unsecure));


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

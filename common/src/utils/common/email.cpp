#include "email.h"

#include <QRegExp>
#include <QHash>

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
    const int TIMEOUT = 20; //seconds

    static void addPresets(const QStringList &domains, QnEmail::SmtpServerPreset server, QnSmtpPresets* presets) {
        foreach(QString domain, domains)
            presets->insert(domain, server);
    }


    //TODO: #GDM check authorization login: use domain or not
    // Actual list here:
    // https://noptix.enk.me/redmine/projects/vms/wiki/SMTP_Server_Presets
    static QnSmtpPresets initSmtpPresets() {
        typedef QLatin1String _;
        typedef QnEmail::SmtpServerPreset server;

        QnSmtpPresets presets;
        presets.insert(_("gmail.com"),          server(_("smtp.gmail.com")));

        presets.insert(_("dwcc.tv"),            server(_("mail.dwcc.tv"), QnEmail::Unsecure, 587));
//        presets.insert(_("networkoptix.com"),   server(_("mail.ex2.secureserver.net")));

        presets.insert(_("yahoo.com"),          server(_("smtp.mail.yahoo.com"), QnEmail::Ssl));
        presets.insert(_("yahoo.co.uk"),        server(_("smtp.mail.yahoo.co.uk"), QnEmail::Ssl));
        presets.insert(_("yahoo.de"),           server(_("smtp.mail.yahoo.de"), QnEmail::Ssl));
        presets.insert(_("yahoo.com.au"),       server(_("smtp.mail.yahoo.com.au"), QnEmail::Ssl));

        presets.insert(_("att.yahoo.com"),      server(_("smtp.att.yahoo.com"), QnEmail::Ssl));

        presets.insert(_("o2.ie"),              server(_("smtp.o2.ie"), QnEmail::Unsecure));
        presets.insert(_("o2.co.uk"),           server(_("smtp.o2.co.uk"), QnEmail::Unsecure));
        presets.insert(_("o2online.de"),        server(_("mail.o2online.de"), QnEmail::Unsecure));

        presets.insert(_("inbox.com"),          server(_("my.inbox.com")));
        presets.insert(_("aol.com"),            server(_("smtp.aol.com")));

        presets.insert(_("lycos.com"),          server(_("smtp.mail.lycos.com"), QnEmail::Unsecure));
        presets.insert(_("mail.com"),           server(_("smtp.mail.com"), QnEmail::Ssl));
        presets.insert(_("netscape.com"),       server(_("mail.netscape.ca")));
        presets.insert(_("rogers.com"),         server(_("smtp.broadband.rogers.com")));
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

        presets.insert(_("li.ru"),              server(_("smtp.li.ru"), QnEmail::Ssl));
        presets.insert(_("nic.ru"),             server(_("mail.nic.ru"), QnEmail::Unsecure));

        addPresets(QStringList()
                   << _("sympatico.ca")
                   << _("bell.net"),
                   server(_("smtphm.sympatico.ca"), QnEmail::Ssl, 25), &presets);       //TODO: Check this strange server

        addPresets(QStringList()
                   << _("telus.net")
                   << _("shaw.ca")
                   << _("videotron.ca")
                   << _("cogeco.ca"),
                   server(_("smtp.telus.net"), QnEmail::Unsecure), &presets);

        addPresets(QStringList()
                   << _("hotmail.co.uk")
                   << _("hotmail.com")
                   << _("outlook.com")
                   << _("msn.com"),
                   server(_("smtp.live.com")), &presets);

        addPresets(QStringList()
                   << _("mail.ru")
                   << _("list.ru")
                   << _("bk.ru")
                   << _("inbox.ru"),
                   server(_("smtp.mail.ru"), QnEmail::Ssl), &presets);

        addPresets(QStringList()
                   << _("narod.ru")
                   << _("yandex.ru"),
                   server(_("smtp.yandex.ru"), QnEmail::Ssl), &presets);

        addPresets(QStringList()
                   << _("rambler.ru")
                   << _("lenta.ru")
                   << _("autorambler.com")
                   << _("myrambler.ru")
                   << _("ro.ru")
                   << _("r0.ru"),
                   server(_("mail.rambler.ru")), &presets);

        addPresets(QStringList()
                   << _("qip.ru")
                   << _("pochta.ru")
                   << _("fromru.com")
                   << _("front.ru")
                   << _("hotbox.ru")
                   << _("hotmail.ru")
                   << _("krovatka.su")
                   << _("land.ru")
                   << _("mail15.com")
                   << _("mail333.com")
                   << _("newmail.ru")
                   << _("nightmail.ru")
                   << _("nm.ru")
                   << _("pisem.net")
                   << _("pochtamt.ru")
                   << _("pop3.ru")
                   << _("rbcmail.ru")
                   << _("smtp.ru")
                   << _("5ballov.ru")
                   << _("aeterna.ru")
                   << _("ziza.ru")
                   << _("memori.ru")
                   << _("photofile.ru")
                   << _("fotoplenka.ru"),
                   server(_("smtp.qip.ru"), QnEmail::Unsecure), &presets);

        addPresets(QStringList()
                   << _("km.ru")
                   << _("freemail.ru")
                   << _("bossmail.com")
                   << _("girlmail.ru")
                   << _("boymail.ru")
                   << _("safebox.ru")
                   << _("megabox.ru"),
                   server(_("smtp.km.ru"), QnEmail::Unsecure), &presets);

        return presets;
    }

    static const QnSmtpPresets SmtpServerPresetPresets = initSmtpPresets();
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
    return SmtpServerPreset();
}

QnEmail::Settings QnEmail::settings() const {
    Settings result;
    SmtpServerPreset preset = smtpServer();

    result.server = preset.server;
    result.port = preset.port;
//    == 0
//            ? QnEmail::defaultPort(preset.connectionType)
//            : preset.port;
    result.connectionType = preset.connectionType;

    return result;
}

QnEmail::Settings::Settings():
    timeout(TIMEOUT) {}

QnEmail::Settings::Settings(const QnKvPairList &values):
    timeout(TIMEOUT)
{
    bool useTls = true;
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

QnKvPairList QnEmail::Settings::serialized() const {
    QnKvPairList result;

    bool useTls = connectionType == QnEmail::Tls;
    bool useSsl = connectionType == QnEmail::Ssl;

    result
    << QnKvPair(nameHost, server)
    << QnKvPair(namePort, port == 0 ? defaultPort(connectionType) : port)
    << QnKvPair(nameUser, user)
    << QnKvPair(nameFrom, user)
    << QnKvPair(namePassword, password)
    << QnKvPair(nameTls, useTls)
    << QnKvPair(nameSsl, useSsl)
    << QnKvPair(nameSimple, simple)
    << QnKvPair(nameTimeout, TIMEOUT);
    return result;

}

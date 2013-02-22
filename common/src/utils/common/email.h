#ifndef EMAIL_H
#define EMAIL_H

#include <QString>
#include <QStringList>

struct QnSmptServerPreset {
    QnSmptServerPreset() {}
    QnSmptServerPreset(const QString &server, const bool useTls = true, const int port = 0);

    bool isNull() const {
        return server.isEmpty();
    }

    QString server;
    bool useTls;
    int port;
};

class QnEmail {
public:
    QnEmail(const QString &email);

    static bool isValid(const QString &email);

    static const int securePort = 587;
    static const int unsecurePort = 25;

    bool isValid() const;
    QnSmptServerPreset smptServer() const;
    QString domain() const;
private:
    QString m_email;
};


#endif // EMAIL_H

#ifndef EMAIL_H
#define EMAIL_H

#include <QString>
#include <QStringList>

// TODO: #GDM
/*

class QnEmail {
public:
    QnEmail();
    QnEmail(const QString &);

    bool isValid();

    QString domain() const;
    QString user() const;
}

Would be much nicer than a bunch of free-standing functions.

*/

bool isEmailValid(const QString &email);
QString getEmailDomain(const QString &email);
QString getEmailUser(const QString &email);


#endif // EMAIL_H

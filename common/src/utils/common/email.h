#ifndef EMAIL_H
#define EMAIL_H

#include <QString>
#include <QStringList>

bool isEmailValid(const QString &email);
QString getEmailDomain(const QString &email);

#endif // EMAIL_H

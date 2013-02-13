#ifndef EMAIL_H
#define EMAIL_H

#include <QString>
#include <QStringList>

bool isEmailValid(const QString &email);
bool isEmailValid(const QStringList& emails);

#endif // EMAIL_H

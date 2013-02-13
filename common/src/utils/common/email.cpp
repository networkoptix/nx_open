#include "email.h"

#include <QRegExp>

const QLatin1String emailPattern("\\b[A-Z0-9._%+-]+@[A-Z0-9.-]+\\.[A-Z]{2,4}\\b");

bool isEmailValid(const QString &email) {
    QRegExp rx(emailPattern);
    return rx.exactMatch(email.toUpper());
}

bool isEmailValid(const QStringList &emails) {
    foreach (const QString &email, emails)
        if (!isEmailValid(email))
            return false;
    return true;
}

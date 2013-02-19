#include "email.h"

#include <QRegExp>

const QLatin1String emailPattern("\\b[A-Z0-9._%+-]+@[A-Z0-9.-]+\\.[A-Z]{2,4}\\b");

bool isEmailValid(const QString &email) {
    QRegExp rx(emailPattern);
    return rx.exactMatch(email.toUpper());
}

QString getEmailDomain(const QString &email) {
    int idx = email.indexOf(QLatin1Char('@'));
    return email.mid(idx + 1).trimmed();
}

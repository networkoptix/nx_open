#include "lexical_functions.h"

void serialize(const bool &value, QString *target) {
    *target = value ? lit("true") : lit("false");
}

bool deserialize(const QString &value, bool *target) {
    /* We also support upper/lower case combinations and "0" & "1" 
     * during deserialization. */

    if(value.isEmpty())
        return false;

    switch (value[0].unicode()) {
    case L'0':
        if(value.size() != 1) {
            return false;
        } else {
            *target = false;
            return true;
        }
    case L'1':
        if(value.size() != 1) {
            return false;
        } else {
            *target = true;
            return true;
        }
    case L't':
    case L'T':
        if(value.size() != 4) {
            return false;
        } else {
            const qunicodechar *data = reinterpret_cast<const qunicodechar *>(value.data());
            const qunicodechar *rueData = QT_UNICODE_LITERAL("rue");
            if(memcmp(data + 1, rueData, sizeof(qunicodechar) * 3) == 0) {
                *target = true;
                return true;
            } else if((data[1] == L'r' || data[1] == L'R') && (data[2] == L'u' || data[2] == L'U') && (data[3] == L'e' || data[3] == L'E')) {
                *target = true;
                return true;
            } else {
                return false;
            }
        }
    case L'f':
    case L'F':
        if(value.size() != 5) {
            return false;
        } else {
            const qunicodechar *data = reinterpret_cast<const qunicodechar *>(value.data());
            const qunicodechar *alseData = QT_UNICODE_LITERAL("alse");
            if(memcmp(data + 1, alseData, sizeof(qunicodechar) * 4) == 0) {
                *target = false;
                return true;
            } else if((data[1] == L'a' || data[1] == L'A') && (data[2] == L'l' || data[2] == L'L') && (data[3] == L's' || data[3] == L'S') && (data[4] == L'e' || data[4] == L'E')) {
                *target = false;
                return true;
            } else {
                return false;
            }
        }
    default:
        return false;
    }
}

void serialize(const QnUuid &value, QString *target) {
    *target = value.toString();
}

bool deserialize(const QString &value, QnUuid *target) {
    QnUuid result(value);
    if(result.isNull() && value != lit("00000000-0000-0000-0000-000000000000") && value != lit("{00000000-0000-0000-0000-000000000000}"))
        return false;

    *target = result;
    return true;
}

void serialize(const QUrl &value, QString *target) {
    *target = value.toString();
}

bool deserialize(const QString &value, QUrl *target) {
    *target = QUrl(value);
    return target->isValid();
}

void serialize(const QColor &value, QString *target) {
    *target = value.name();
}

bool deserialize(const QString &value, QColor *target) {
    QString trimmedName = value.trimmed();
    if(trimmedName.startsWith(lit("QColor"))) {
        /* QColor(R, G, B, A) format. */
        trimmedName = trimmedName.mid(trimmedName.indexOf(L'(') + 1);
        trimmedName = trimmedName.left(trimmedName.lastIndexOf(L')'));

        QStringList args = trimmedName.split(L',');
        if(args.size() < 3 || args.size() > 4)
            return false;

        QList<int> colors;
        foreach(const QString &arg, args) {
            bool ok = false;
            int color = arg.toInt(&ok);
            if(!ok)
                return false;
            colors.push_back(color);
        }

        QColor result(colors[0], colors[1], colors[2]);
        if(colors.size() == 4)
            result.setAlpha(colors[3]);

        *target = result;
        return true;
    } else if(trimmedName.startsWith(L'#') && trimmedName.size() == 9) {
        /* #RRGGBBAA format. */

        QColor result(trimmedName.left(7));
        if(!result.isValid())
            return false;

        bool success = false;
        int alpha = trimmedName.right(2).toInt(&success, 16);
        if(!success)
            return false;

        result.setAlpha(alpha);
        *target = result;
        return true;
    } else {
        /* Named format. */
        QColor result(trimmedName);
        if(result.isValid()) {
            *target = result;
            return true;
        } else {
            return false;
        }
    }
}


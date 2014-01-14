#include "lexical_functions.h"

void serialize(const bool &value, QString *target) {
    *target = value ? lit("true") : lit("false");
}

bool deserialize(const QString &value, bool *target) {
    /* Also support "0" & "1" during deserialization. */

    if(value == lit("0") || value == lit("false")) {
        *target = false;
        return true;
    } else if(value == lit("1") || value == lit("true")) {
        *target = true;
        return true;
    } else {
        return false;
    }
}

void serialize(const QUuid &value, QString *target) {
    *target = value.toString();
}

bool deserialize(const QString &value, QUuid *target) {
    /* Also support "0" & "1" during deserialization. */

    QUuid result(value);
    if(result.isNull() && value != lit("00000000-0000-0000-0000-000000000000") && value != lit("{00000000-0000-0000-0000-000000000000}"))
        return false;

    *target = result;
    return true;
}

void serialize(const QColor &value, QString *target) {
    *target = value.name();
}

bool deserialize(const QString &value, QColor *target) {
    QString trimmedName = value.trimmed();
    if(trimmedName.startsWith(QLatin1String("QColor"))) {
        /* QColor(R, G, B, A) format. */
        trimmedName = trimmedName.mid(trimmedName.indexOf(QLatin1Char('(')) + 1);
        trimmedName = trimmedName.left(trimmedName.lastIndexOf(QLatin1Char(')')));

        QStringList args = trimmedName.split(QLatin1Char(','));
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


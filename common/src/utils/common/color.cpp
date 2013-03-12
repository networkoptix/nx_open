#include "color.h"

QColor parseColor(const QString &name, const QColor &defaultValue) {
    QString trimmedName = name.trimmed();
    if(trimmedName.startsWith(QLatin1String("QColor"))) {
        trimmedName = trimmedName.mid(trimmedName.indexOf(QLatin1Char('(')) + 1);
        trimmedName = trimmedName.left(trimmedName.lastIndexOf(QLatin1Char(')')));

        QStringList args = trimmedName.split(QLatin1Char(','));
        if(args.size() < 3 || args.size() > 4)
            return defaultValue;

        QList<int> colors;
        foreach(const QString &arg, args) {
            bool ok = false;
            int color = arg.toInt(&ok);
            if(!ok)
                return defaultValue;
            colors.push_back(color);
        }

        QColor result(colors[0], colors[1], colors[2]);
        if(colors.size() == 4)
            result.setAlpha(colors[3]);

        return result;
    } else {
        QColor result(trimmedName);
        if(result.isValid()) {
            return result;
        } else {
            return defaultValue;
        }
    }
}

QColor parseColor(const QVariant &value, const QColor &defaultValue) {
    if(value.userType() == QVariant::Color) {
        return value.value<QColor>();
    } else if(value.userType() == QVariant::StringList) {
        return parseColor(value.toStringList().join(QLatin1String(",")), defaultValue);
    } else {
        return parseColor(value.toString(), defaultValue);
    }
}

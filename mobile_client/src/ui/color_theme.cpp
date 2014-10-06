#include "color_theme.h"

#include <QtCore/QDebug>
#include <QtCore/QVariant>
#include <QtCore/QFile>
#include <QtCore/QRegExp>
#include <QtCore/qjsondocument.h>

namespace {

QRegExp colorRegExp(lit("#([\\da-fA-F]{2}){3,4}|") + QColor::colorNames().join(QLatin1Char('|')));
QColor warningColor = Qt::magenta;

QVariantMap readColorsMap(const QString &fileName) {
    QFile file(fileName);
    if (!file.open(QFile::ReadOnly)) {
        qWarning() << "Could not open color theme file" << fileName;
        return QVariantMap();
    }

    QByteArray data = file.readAll();

    QJsonParseError error;
    QJsonDocument document = QJsonDocument::fromJson(data, &error);

    if (error.error != QJsonParseError::NoError) {
        int start = data.lastIndexOf('\n', error.offset);
        int end = data.indexOf('\n', error.offset);
        if (start == -1)
            start = 0;
        if (end == -1)
            end = data.size() - 1;

        int pos = error.offset - start;
        int line = data.left(error.offset).count('\n') + 1;

        qWarning() << "QnColorTheme" << line << ":" << pos << ":" << error.errorString();
        return QVariantMap();
    }

    return document.toVariant().toMap();
}

bool isColor(const QString &value) {
    return colorRegExp.exactMatch(value);
}

QHash<QString, QColor> parseColors(const QVariantMap &colorsMap, QHash<QString, QColor> parsed) {
    QVariantMap pending;

    for (auto it = colorsMap.begin(); it != colorsMap.end(); ++it) {
        QString value = it.value().toString();
        QColor color = warningColor;

        if (isColor(value)) {
            color = QColor(value);
        } else if (parsed.contains(value)) {
            color = parsed.value(value);
        } else {
            pending.insert(it.key(), it.value());
            continue;
        }

        parsed.insert(it.key(), color);
    }

    /* nothing parsed: set fallback color */
    if (pending.size() == colorsMap.size()) {
        for (auto it = pending.begin(); it != pending.end(); ++it) {
            qWarning() << "QnColorTheme: Couldn't find color for" << it.key() << ":" << it.value().toString();
            parsed.insert(it.key(), warningColor);
        }
    } else if (pending.size() > 0) {
        return parseColors(pending, parsed);
    }

    return parsed;
}

QHash<QString, QColor> parseColors(const QVariantMap &colorsMap) {
    return parseColors(colorsMap, QHash<QString, QColor>());
}

} // anonymous namespace

QnColorTheme::QnColorTheme(QObject *parent) :
    QObject(parent)
{
}

void QnColorTheme::readFromFile(const QString &fileName) {
    QVariantMap colorsMap = readColorsMap(fileName);
    if (colorsMap.isEmpty())
        return;

    m_colors = parseColors(colorsMap);
}

QColor QnColorTheme::color(const QString &key) const {
    return m_colors.value(key);
}

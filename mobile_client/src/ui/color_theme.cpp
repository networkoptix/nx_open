#include "color_theme.h"

#include <QtCore/QDebug>
#include <QtCore/QVariant>
#include <QtCore/QFile>
#include <QtCore/QRegExp>
#include <QtCore/qjsondocument.h>

namespace {

QRegExp colorRegExp(lit("#([\\da-fA-F]{2}){3,4}|") + QColor::colorNames().join(QLatin1Char('|')));
QColor warningColor = Qt::magenta;

QVariantMap plainify(const QVariantMap &map, const QString &prefix = QString()) {
    QVariantMap result;

    for (auto it = map.begin(); it != map.end(); ++it) {
        if (it.value().type() == QVariant::Map) {
            QVariantMap submap = plainify(it.value().toMap(), prefix + it.key() + lit("."));
            for (auto jt = submap.begin(); jt != submap.end(); ++jt)
                result.insert(jt.key(), jt.value());
        } else {
            result.insert(prefix + it.key(), it.value());
        }
    }

    return result;
}

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

    return plainify(document.toVariant().toMap());
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

QPalette::ColorGroup colorGroup(const QString &groupName) {
    if (groupName == lit("active"))
        return QPalette::Active;
    else if (groupName == lit("inactive"))
        return QPalette::Inactive;
    else if (groupName == lit("disabled"))
        return QPalette::Disabled;

    return QPalette::Normal;
}

QPalette::ColorRole colorRole(const QString &colorRole) {
    static QHash<QString, QPalette::ColorRole> nameToRoleHash;
    if (nameToRoleHash.isEmpty()) {
        nameToRoleHash[lit("windowText")]       = QPalette::WindowText;
        nameToRoleHash[lit("button")]           = QPalette::Button;
        nameToRoleHash[lit("light")]            = QPalette::Light;
        nameToRoleHash[lit("midlight")]         = QPalette::Midlight;
        nameToRoleHash[lit("dark")]             = QPalette::Dark;
        nameToRoleHash[lit("mid")]              = QPalette::Mid;
        nameToRoleHash[lit("text")]             = QPalette::Text;
        nameToRoleHash[lit("brightText")]       = QPalette::BrightText;
        nameToRoleHash[lit("buttonText")]       = QPalette::ButtonText;
        nameToRoleHash[lit("base")]             = QPalette::Base;
        nameToRoleHash[lit("window")]           = QPalette::Window;
        nameToRoleHash[lit("shadow")]           = QPalette::Shadow;
        nameToRoleHash[lit("highlight")]        = QPalette::Highlight;
        nameToRoleHash[lit("highlightedText")]  = QPalette::HighlightedText;
        nameToRoleHash[lit("link")]             = QPalette::Link;
        nameToRoleHash[lit("linkVisited")]      = QPalette::LinkVisited;
        nameToRoleHash[lit("alternateBase")]    = QPalette::AlternateBase;
        nameToRoleHash[lit("toolTipBase")]      = QPalette::ToolTipBase;
        nameToRoleHash[lit("linkVisited")]      = QPalette::LinkVisited;
    }
    return nameToRoleHash.value(colorRole, QPalette::NoRole);
}

void fillPalette(QPalette &palette, const QHash<QString, QColor> &colorsMap) {
    QRegExp paletteColorRegExp(lit("palette\\.(active|inactive|disabled)\\.(.+)"));
    for (auto it = colorsMap.begin(); it != colorsMap.end(); ++it) {
        if (!paletteColorRegExp.exactMatch(it.key()))
            continue;
        QPalette::ColorGroup group = colorGroup(paletteColorRegExp.cap(1));
        QPalette::ColorRole role = colorRole(paletteColorRegExp.cap(2));

        if (role == QPalette::NoRole)
            continue;

        palette.setColor(group, role, it.value());
    }
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
    fillPalette(m_palette, m_colors);
}

QColor QnColorTheme::color(const QString &key) const {
    QColor color = m_colors.value(key);
    if (!color.isValid()) {
        qWarning() << "QnColorTheme: requested color does not exist:" << key;
        return warningColor;
    }

    return color;
}

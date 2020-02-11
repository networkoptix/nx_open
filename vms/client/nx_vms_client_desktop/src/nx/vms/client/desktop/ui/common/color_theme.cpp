#include "color_theme.h"

#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QVariant>
#include <QtCore/QRegularExpression>
#include <QtGui/QColor>
#include <QtQml/QtQml>

#include <nx/utils/log/log.h>

static uint qHash(const QColor& color)
{
    return color.rgb();
}

namespace nx::vms::client::desktop {

struct ColorTheme::Private
{
    QVariantMap colors;

    struct Group
    {
        QList<QColor> colors; //< Sorted by lightness.
        QStringList keys; //< Sorted alphabetically.
    };

    QHash<QString, Group> groups;

    QMap<QString, QString> colorSubstitutions; //< Default #XXXXXX to updated #YYYYYY.

    struct ColorInfo
    {
        QString group;
        int index = -1;

        ColorInfo() = default;
        ColorInfo(const QString& group, int index): group(group), index(index) {}
    };

    QHash<QColor, ColorInfo> colorInfoByColor;

    /** Initialize color values, color groups and color substitutions. */
    void loadColors(
        const QString& mainColorsFile = ":/skin/customization_common.json",
        const QString& skinColorsFile = ":/skin/skin.json");

private:
    QJsonObject readColorDataFromFile(const QString& fileName) const;
    QSet<QString> updateColors(const QJsonObject& newColors);
};

void ColorTheme::Private::loadColors(const QString& mainColorsFile, const QString& skinColorsFile)
{
    // Load base colors and override them with the actual skin values.

    const QJsonObject baseColors = readColorDataFromFile(mainColorsFile);
    updateColors(baseColors);

    const QVariantMap defaultColors = colors;

    const QJsonObject currentSkinColors = readColorDataFromFile(skinColorsFile);
    const QSet<QString> updatedColors = updateColors(currentSkinColors);

    // Calculate color substitutions.

    for (auto updatedColorName: updatedColors)
    {
        if (defaultColors.contains(updatedColorName))
        {
            colorSubstitutions[defaultColors[updatedColorName].value<QColor>().name()] =
                colors[updatedColorName].value<QColor>().name();
        }
    }
}

QJsonObject ColorTheme::Private::readColorDataFromFile(const QString& fileName) const
{
    QFile file(fileName);
    const bool opened = file.open(QFile::ReadOnly);
    if (NX_ASSERT(opened, "Cannot read skin file %1", fileName))
    {
        const auto& jsonData = file.readAll();

        file.close();

        QJsonParseError error;
        const auto& json = QJsonDocument::fromJson(jsonData, &error);

        const bool parsed = error.error == QJsonParseError::NoError;
        if (NX_ASSERT(parsed, "JSON parse error: %1", error.errorString())
                && NX_ASSERT(json.isObject(), "Invalid JSON structure"))
        {
            return json.object().value("globals").toObject();
        }
    }

    return QJsonObject();
}

QSet<QString> ColorTheme::Private::updateColors(const QJsonObject& newColors)
{
    const QRegularExpression groupNameRe("([^_\\d]+)");

    QSet<QString> updatedColors;

    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);

    const auto insertAlphabetically =
        [&collator](QStringList& list, const QString& key)
        {
            auto iter = std::lower_bound(list.begin(), list.end(), key,
                [&collator](const QString& left, const QString& right)
                {
                    return collator.compare(left, right) < 0;
                });

            if (iter == list.end() || *iter != key)
                list.insert(iter, key);
        };

    for (auto it = newColors.begin(); it != newColors.end(); ++it)
    {
        if (it->type() != QJsonValue::String)
            continue;

        const auto& colorName = it.key();
        const auto& color = QColor(it.value().toString());
        if (!color.isValid())
            continue;

        const QColor oldColor = colors[colorName].value<QColor>();
        colors[colorName] = color;

        if (oldColor.isValid())
            updatedColors.insert(colorName);

        if (auto groupNameMatch = groupNameRe.match(colorName); groupNameMatch.hasMatch())
        {
            const QString groupName = groupNameMatch.captured(0);
            auto& group = groups[groupName];
            if (oldColor.isValid())
                group.colors.removeOne(oldColor);

            group.colors.append(color);
            insertAlphabetically(group.keys, colorName);
        }
    }

    for (auto it = groups.begin(); it != groups.end(); ++it)
    {
        auto& colors = it.value().colors;

        std::sort(colors.begin(), colors.end(),
            [](const QColor& c1, const QColor& c2)
            {
                return c1.toHsl().lightness() < c2.toHsl().lightness();
            });

        for (int i = 0; i < colors.size(); ++i)
            colorInfoByColor[colors[i]] = ColorInfo{it.key(), i};
    }

    return updatedColors;
}

ColorTheme::ColorTheme(QObject* parent):
    QObject(parent),
    d(new Private())
{
    d->loadColors();
}

ColorTheme::ColorTheme(
    const QString& mainColorsFile,
    const QString& skinColorsFile,
    QObject* parent)
    :
    QObject(parent),
    d(new Private())
{
    d->loadColors(mainColorsFile, skinColorsFile);
}

ColorTheme::~ColorTheme()
{
}

QVariantMap ColorTheme::colors() const
{
    return d->colors;
}

QMap<QString, QString> ColorTheme::getColorSubstitutions() const
{
    return d->colorSubstitutions;
}

QColor ColorTheme::color(const char* name) const
{
    return d->colors.value(QLatin1String(name)).value<QColor>();
}

QColor ColorTheme::color(const QString& name) const
{
    return d->colors.value(name).value<QColor>();
}

QColor ColorTheme::color(const char* name, qreal alpha) const
{
    return transparent(color(name), alpha);
}

QColor ColorTheme::color(const QString& name, qreal alpha) const
{
    return transparent(color(name), alpha);
}

QList<QColor> ColorTheme::groupColors(const char* groupName) const
{
    return groupColors(QLatin1String(groupName));
}

QList<QColor> ColorTheme::groupColors(const QString& groupName) const
{
    return d->groups[groupName].colors;
}

QStringList ColorTheme::groupKeys(const QString& groupName) const
{
    return d->groups[groupName].keys;
}

QColor ColorTheme::transparent(const QColor& color, qreal alpha)
{
    auto newColor = color;
    newColor.setAlphaF(alpha);
    return newColor;
}

QColor ColorTheme::darker(const QColor& color, int offset) const
{
    return lighter(color, -offset);
}

QColor ColorTheme::lighter(const QColor& color, int offset) const
{
    const auto& info = d->colorInfoByColor.value(transparent(color, 1.0));

    if (info.group.isEmpty())
    {
        auto hsl = color.toHsl();
        hsl.setHsl(
            hsl.hslHue(),
            hsl.hslSaturation(),
            qBound(0, hsl.lightness() + offset * 10, 255));
        return hsl.toRgb();
    }

    const auto& colors = d->groups.value(info.group).colors;
    auto result = colors[qBound(0, info.index + offset, colors.size() - 1)];
    result.setAlpha(color.alpha());
    return result;
}

Q_INVOKABLE bool ColorTheme::isDark(const QColor& color)
{
    return color.toHsv().value() < 128;
}

Q_INVOKABLE bool ColorTheme::isLight(const QColor& color)
{
    return !isDark(color);
}

void ColorTheme::registerQmlType()
{
    qmlRegisterType<ColorTheme>("Nx", 1, 0, "ColorThemeBase");
}

} // namespace nx::vms::client::desktop

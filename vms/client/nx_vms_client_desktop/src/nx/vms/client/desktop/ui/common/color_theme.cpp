#include "color_theme.h"

#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QVariant>
#include <QtCore/QRegularExpression>
#include <QtGui/QColor>

#include <ui/style/skin.h>

#include <nx/utils/log/log.h>

static uint qHash(const QColor& color)
{
    return color.rgb();
}

template<>
nx::vms::client::desktop::ColorTheme* Singleton<nx::vms::client::desktop::ColorTheme>::s_instance =
    nullptr;

namespace nx::vms::client::desktop {

static const auto kBaseSkinFileName = "customization_common.json";
static const auto kCustomSkinFileName = "skin.json";

struct ColorTheme::Private
{
    QVariantMap colors;

    QHash<QString, QList<QColor>> groups;

    QMap<QString, QString> colorSubstitutions;  //< Default #XXXXXX to updated #YYYYYY.

    struct ColorInfo
    {
        QString group;
        int index = -1;

        ColorInfo() = default;
        ColorInfo(const QString& group, int index): group(group), index(index) {}
    };

    QHash<QColor, ColorInfo> colorInfoByColor;

    void loadColors();

private:
    QJsonObject readColorsFromFile(const QString& filename) const;
    QSet<QString> updateColors(const QJsonObject& newColors);
};

void ColorTheme::Private::loadColors()
{
    // Load base colors and override them with the actual skin values.

    const QJsonObject baseColors = readColorsFromFile(kBaseSkinFileName);
    updateColors(baseColors);

    const QVariantMap defaultColors = colors;

    const QJsonObject currentSkinColors = readColorsFromFile(kCustomSkinFileName);
    const QSet<QString> updatedColors = updateColors(currentSkinColors);

    for (auto it = updatedColors.begin(); it != updatedColors.end(); ++it)
    {
        if (defaultColors.contains(*it))
        {
            colorSubstitutions[defaultColors[*it].value<QColor>().name()] =
                colors[*it].value<QColor>().name();
        }
    }
}

QJsonObject ColorTheme::Private::readColorsFromFile(const QString& filename) const
{
    QJsonObject result;

    QFile file(qnSkin->path(filename));
    const bool opened = file.open(QFile::ReadOnly);
    if (NX_ASSERT(opened, "Cannot read skin file %1", filename))
    {
        const auto& jsonData = file.readAll();

        file.close();

        QJsonParseError error;
        const auto& json = QJsonDocument::fromJson(jsonData, &error);

        const bool parsed = error.error == QJsonParseError::NoError;
        if (NX_ASSERT(parsed, "JSON parse error: %1", error.errorString())
                && NX_ASSERT(json.isObject(), "Invalid JSON structure"))
            return json.object().value("globals").toObject();
    }

    return result;
}

QSet<QString> ColorTheme::Private::updateColors(const QJsonObject& newColors)
{
    const QRegularExpression groupNameRe("([^_\\d]+)");

    QSet<QString> updatedColors;

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

        QRegularExpressionMatch groupNameMatch = groupNameRe.match(colorName);
        if (groupNameMatch.hasMatch())
        {
            const QString groupName = groupNameMatch.captured(0);
            QList<QColor>& group = groups[groupName];
            if (oldColor.isValid())
                group.removeOne(oldColor);
            groups[groupName].append(color);
        }
    }

    for (auto it = groups.begin(); it != groups.end(); ++it)
    {
        auto& colors = it.value();

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
    return d->groups[QLatin1String(groupName)];
}

QList<QColor> ColorTheme::groupColors(const QString& groupName) const
{
    return d->groups[groupName];
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

    const auto& colors = d->groups.value(info.group);
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

} // namespace nx::vms::client::desktop

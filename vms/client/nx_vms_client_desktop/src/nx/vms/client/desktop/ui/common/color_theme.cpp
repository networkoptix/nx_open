// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "color_theme.h"

#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QVariant>
#include <QtGui/QColor>
#include <QtQml/QtQml>

#include <utils/common/hash.h>

#include <utils/math/color_transformations.h>

#include <nx/utils/log/log.h>

#include "color_theme_reader.h"

namespace nx::vms::client::desktop {

static ColorTheme* s_instance = nullptr;

struct ColorTheme::Private
{
    ColorTree::ColorMap colorsByPath;
    QVariantMap colorTree;

    ColorTree::ColorGroups groups;

    ColorSubstitutions colorSubstitutions; //< Default #XXXXXX to updated #YYYYYY.

    QHash<QColor, ColorInfo> colorInfoByColor;

    /** Initialize color values, color groups and color substitutions. */
    void loadColors(
        const QString& mainColorsFile = ":/skin/basic_colors.json",
        const QString& skinColorsFile = ":/skin/skin_colors.json");

private:
    QJsonObject readColorDataFromFile(const QString& fileName) const;
};

void ColorTheme::Private::loadColors(const QString& mainColorsFile, const QString& skinColorsFile)
{
    // Load base colors and override them with the actual skin values.

    ColorThemeReader reader;

    const QJsonObject baseColors = readColorDataFromFile(mainColorsFile);
    reader.addColors(baseColors);

    ColorTree baseColorTree = reader.getColorTree();

    const QJsonObject currentSkinColors = readColorDataFromFile(skinColorsFile);
    reader.addColors(currentSkinColors);

    ColorTree actualColorTree = reader.getColorTree();

    colorTree = actualColorTree.toVariantMap();
    colorsByPath = actualColorTree.colorsByPath();

    colorSubstitutions = baseColorTree.rootColorsDelta(actualColorTree);

    actualColorTree.baseColorGroups(&groups, &colorInfoByColor);
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
            return json.object();
        }
    }

    return QJsonObject();
}

ColorTheme::ColorTheme(QObject* parent):
    QObject(parent),
    d(new Private())
{
    if (s_instance)
        NX_ERROR(this, "Singleton is created more than once.");
    else
        s_instance = this;

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
    if (s_instance)
        NX_ERROR(this, "Singleton is created more than once.");
    else
        s_instance = this;

    d->loadColors(mainColorsFile, skinColorsFile);
}

ColorTheme::~ColorTheme()
{
    if (s_instance == this)
        s_instance = nullptr;
}

ColorTheme* ColorTheme::instance()
{
    return s_instance;
}

QVariantMap ColorTheme::colors() const
{
    return d->colorTree;
}

ColorSubstitutions ColorTheme::getColorSubstitutions() const
{
    return d->colorSubstitutions;
}

QColor ColorTheme::color(const QString& name) const
{
    NX_ASSERT(d->colorsByPath.contains(name));
    const ColorTree::ColorData colorData = d->colorsByPath.value(name);

    if (!NX_ASSERT(std::holds_alternative<QColor>(colorData)))
        return QColor();

    return std::get<QColor>(colorData);
}

QColor ColorTheme::color(const QString& name, std::uint8_t alpha) const
{
    NX_ASSERT(alpha > 0, "Make sure you have not passed floating-point transparency value here");
    return withAlpha(color(name), alpha);
}

QList<QColor> ColorTheme::colors(const QString& name) const
{
    NX_ASSERT(d->colorsByPath.contains(name));
    const ColorTree::ColorData colorData = d->colorsByPath.value(name);

    if (!NX_ASSERT(std::holds_alternative<QList<QColor>>(colorData)))
        return QList<QColor>();

    return std::get<QList<QColor>>(colorData);
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

void ColorTheme::registerQmlType()
{
    qmlRegisterType<ColorTheme>("Nx", 1, 0, "ColorThemeBase");
}

} // namespace nx::vms::client::desktop

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
#include <nx/utils/qt_helpers.h>
#include <utils/common/hash.h>
#include <utils/math/color_transformations.h>

#include "color_theme_reader.h"
#include "icon.h"

namespace nx::vms::client::core {

static ColorTheme* s_instance = nullptr;

// ------------------------------------------------------------------------------------------------
// ColorTheme::Private

struct ColorTheme::Private
{
    ColorTheme* const q;

    ColorTree::ColorMap colorsByPath;
    QVariantMap colorTree;
    mutable QJSValue jsColorTree;

    ColorTree::ColorGroups groups;

    ColorSubstitutions colorSubstitutions; //< Default #XXXXXX to updated #YYYYYY.

    QHash<QColor, ColorInfo> colorInfoByColor;

    QHash<QString, IconCustomization> iconCustomizationByCategory;

    /** Initialize color values, color groups and color substitutions. */
    void loadColors(const QString& mainColorsFile, const QString& skinColorsFile);

    void initIconCustomizations();

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
    if (file.open(QFile::ReadOnly))
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

void ColorTheme::Private::initIconCustomizations()
{
    // TODO: VMS-41294 #pprivalov Fix this record by specification, and implement other categories.

    iconCustomizationByCategory.insert("text_buttons", {
        {{QIcon::Off, QIcon::Normal}, {{"primary", "light14"}}},
        {{QIcon::Off, QIcon::Active}, {{"primary", "red"}}},
        {{QIcon::Off, QnIcon::Pressed}, {{"primary", "light14"}}},
        {{QIcon::On, QIcon::Normal}, {{"primary", "light4"}}},
        {{QIcon::On, QIcon::Active}, {{"primary", "light3"}}},
        {{QIcon::On, QnIcon::Pressed}, {{"primary", "light4"}}}});
}

// ------------------------------------------------------------------------------------------------
// ColorTheme

ColorTheme::ColorTheme(QObject* parent):
    ColorTheme(":/skin/basic_colors.json", ":/skin/skin_colors.json")
{
}

ColorTheme::ColorTheme(const QString& basicColorsFileName,
    const QString& skinColorsFileName,
    QObject* parent)
    :
    QObject(parent),
    d(new Private{.q = this})
{
    if (s_instance)
        NX_ERROR(this, "Singleton is created more than once.");
    else
        s_instance = this;

    d->loadColors(basicColorsFileName, skinColorsFileName);
    d->initIconCustomizations();
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

QJSValue ColorTheme::jsColors() const
{
    if (!d->jsColorTree.isUndefined())
        return d->jsColorTree;

    const auto engine = qmlEngine(this);
    if (!NX_ASSERT(engine, "This property should only be accessed from QML code"))
        return {};

    d->jsColorTree = engine->toScriptValue(d->colorTree);
    return d->jsColorTree;
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

QColor ColorTheme::blend(const QColor& background, const QColor& foreground, qreal alpha)
{
    const auto  bg = background.toRgb();
    const auto  fg = foreground.toRgb();

    const auto blend = [](qreal x, qreal y, qreal k) { return x * (1.0 - k) + y * k; };

    return QColor::fromRgbF(
        blend(bg.redF(), fg.redF(), alpha),
        blend(bg.greenF(), fg.greenF(), alpha),
        blend(bg.blueF(), fg.blueF(), alpha),
        blend(bg.alphaF(), fg.alphaF(), alpha));
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

bool ColorTheme::hasIconCustomization(const QString& category) const
{
    return !iconCustomization(category).empty();
}

ColorTheme::IconCustomization ColorTheme::iconCustomization(const QString& category) const
{
    return d->iconCustomizationByCategory.value(category);
}

QMap<QString, QString> ColorTheme::iconImageCustomization(
    const QString& category,
    const QIcon::Mode mode,
    const QIcon::State state) const
{
    const auto customization = iconCustomization(category);
    if (!NX_ASSERT(!customization.empty(),
        "Icon customization category \"%1\" is not defined", category))
    {
        return {};
    }

    const auto result = customization.value({state, mode});
    NX_ASSERT(!result.empty(),
        "Customization of icon category \"%1\" for mode %2 and state %3 is not defined",
        category, mode, state);

    return result;
}

QJSValue ColorTheme::iconImageCustomization(const QString& category,
    bool hovered, bool pressed, bool selected, bool disabled, bool checked) const
{
    const auto engine = qmlEngine(this);
    if (!NX_ASSERT(engine, "This method should only be called from QML code"))
        return {};

    const QIcon::Mode mode =
        [=]()
        {
            if (disabled)
                return QIcon::Disabled;
            if (selected)
                return QIcon::Selected;
            if (pressed)
                return QnIcon::Pressed;
            if (hovered)
                return QIcon::Active;
            return QIcon::Normal;
        }();

    const QIcon::State state = checked ? QIcon::On : QIcon::Off;
    const auto customization = iconImageCustomization(category, mode, state);

    QJSValue result = engine->newObject();
    for (const auto& [name, value]: customization.asKeyValueRange())
        result.setProperty(name, value);
    return result;
}

void ColorTheme::registerQmlType()
{
    qmlRegisterType<ColorTheme>("Nx.Core", 1, 0, "ColorThemeBase");
}

} // namespace nx::vms::client::desktop

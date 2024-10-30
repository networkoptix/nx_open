// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "color_theme.h"

#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QVariant>
#include <QtGui/QColor>
#include <QtQml/QtQml>

#include <nx/branding.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/core/ini.h>
#include <utils/common/hash.h>
#include <utils/math/color_transformations.h>

#include "color_theme_reader.h"
#include "icon.h"

namespace nx::vms::client::core {

namespace {

QStringList kThemeSpecificColors = {
    "red", "pink", "purple", "deep_purple",
    "indigo", "blue", "light_blue", "cyan",
    "teal", "green", "light_green", "lime",
    "yellow", "lemon", "orange", "deep_orange"
};

QString legacyColorsFileName()
{
    return ":/skin/legacy_colors.json";
}

QString basicColorsFileName()
{
    if (auto path = QString::fromUtf8(ini().basicColorsJson); !path.isEmpty())
        return path;

    return ":/skin/basic_colors.json";
}

QString derivedColorsFileName()
{
    return ":/skin/derived_colors.json";
}

QString skinColorsFileName()
{
    const QString iniColorTheme(ini().colorTheme);
    const QString colorTheme = iniColorTheme.isEmpty()
        ? branding::colorTheme()
        : iniColorTheme;

    return QString(":/skin/%1.json").arg(colorTheme);
}

QColor parseColor(const QString& value)
{
    if (value.startsWith('#'))
        return QColor::fromString(value);

    // TODO: Support HSL.

    return {};
}

void initializeThemeSpecificColors(QJsonObject* object)
{
    static const QString kDarkTheme = "dark_theme";
    static const QString kLightTheme = "light_theme";

    // Early return on unit tests.
    if (!object->contains(kDarkTheme) || !object->contains(kLightTheme))
        return;

    const auto theme = ini().invertColors ? kLightTheme : kDarkTheme;
    for (const auto& color: kThemeSpecificColors)
        object->insert(color, QString("%1.%2").arg(theme, color));
}

QJsonObject generatedColorScheme(const ColorTree& basicColors)
{
    const auto kBrandStep = 0.03;
    const auto kBrandBackground = 0.15;

    const auto kColorStep = 0.03;
    const auto kColorBackground = 0.15;

    QJsonObject result;

    QString colorTheme = ini().colorTheme;
    auto primary =
        [&]() -> QColor
        {
            if (colorTheme == "generated" || colorTheme == "rainbow")
                return parseColor(ini().primaryColor);

            auto themeName = colorTheme;

            if (colorTheme == "cms_colors")
            {
                if (auto color = parseColor(nx::branding::brandColor()); color.isValid())
                    return color;

                NX_WARNING(NX_SCOPE_TAG,
                    "Can't parse Primary Color, switching to default theme color.");
                themeName = nx::branding::colorTheme();
            }

            if (colorTheme == "cms_name")
                themeName = nx::branding::colorTheme();

            if (themeName == "dark_green")
                return "#a5ce39";

            if (themeName == "dark_orange")
                return "#ff6c00";

            if (themeName == "gray_orange")
                return "#ff6c00";

            if (themeName == "gray_white")
                return ini().invertColors ? "#000000" : "#ffffff";

            // Default: dark_blue.
            return "#2fa2db";
        }();

    auto background =
        [&]() -> QColor
        {
            if (colorTheme == "generated" || colorTheme == "rainbow")
                return parseColor(ini().backgroundColor);

            auto themeName = colorTheme;

            if (colorTheme == "cms_colors")
            {
                if (auto color = parseColor(nx::branding::brandBgColor()); color.isValid())
                    return color;

                NX_INFO(NX_SCOPE_TAG,
                    "Can't parse Background Color, Primary color will be used for generation.");
                return primary;
            }

            if (colorTheme == "cms_name")
                themeName = nx::branding::colorTheme();

            if (themeName == "dark_green")
                return "#2fa2db";

            if (themeName == "dark_orange")
                return "#2fa2db";

            if (themeName == "gray_orange")
                return "#ffffff";

            if (themeName == "gray_white")
                return "#ffffff";

            // Default: dark_blue.
            return "#2fa2db";
        }();

    if (!primary.isValid())
        return result;

    const auto pH = primary.hslHueF();
    const auto pS = primary.hslSaturationF();
    const auto pL = primary.lightnessF();

    auto brand_core = QColor::fromHslF(pH, pS, pL);
    auto brand_dark = QColor::fromHslF(pH, pS, pL - kBrandStep);
    auto brand_light = QColor::fromHslF(pH, pS, pL + kBrandStep);
    auto brand_bg = QColor::fromHslF(pH, pS, kBrandBackground);

    result.insert("brand", brand_core.name());
    result.insert("brand_core", brand_core.name());
    result.insert("brand_d", brand_dark.name());
    result.insert("brand_l", brand_light.name());
    for (int i = 1; i <= 7; ++i)
        result.insert(QString("brand_d%1").arg(i), brand_dark.name());
    for (int i = 1; i <= 4; ++i)
        result.insert(QString("brand_l%1").arg(i), brand_light.name());
    result.insert("brand_bg", brand_bg.name());

    auto colors = basicColors.getRootColors();
    for (const auto& colorName: kThemeSpecificColors)
    {
        auto color = colors[colorName];
        if (!NX_ASSERT(color.isValid()))
            ;

        const auto cH = color.hslHueF();
        const auto cS = color.hslSaturationF();
        const auto cL = color.lightnessF();

        result.insert(colorName + "_d", QColor::fromHslF(cH, cS, cL - kColorStep).name());
        result.insert(colorName + "_l", QColor::fromHslF(cH, cS, cL + kColorStep).name());
        result.insert(colorName + "_bg", QColor::fromHslF(cH, cS, kColorBackground).name());
    }

    const auto bH = background.isValid() ? background.hslHueF() : pH;
    const auto bS = background.isValid()
        ? (background.hslSaturation() == 0 ? 0 : 0.15)
        : (pS == 0 ? 0 : 0.15);

    const auto darkStep = 0.02; //qBound(0., ini().darkStep, 0.06);
    const auto lightStep = 0.03; //qBound(0., ini().lightStep, 0.06);

    const bool isRainbow = ini().colorTheme == QString("rainbow");

    for (int i = 0; i < 18; ++i)
    {
        QColor dark, light;
        if (isRainbow)
        {
            dark = QColor::fromHslF(i / 18., 0.25, 0.2 + 0.1 * i / 18);
            light = QColor::fromHslF(1 - (i / 18.), 0.75, 0.8 - 0.1 * i / 18);
        }
        else
        {
            dark = QColor::fromHslF(bH, bS, darkStep * i);
            light = QColor::fromHslF(bH, bS, 1 - lightStep * i);
        }

        // Original colors that could be used for Widgets that should always display light text.
        result.insert(QString("@dark%1").arg(i + 1), dark.name());
        result.insert(QString("@light%1").arg(i + 1), light.name());

        if (ini().invertColors)
            qSwap(dark, light);

        result.insert(QString("dark%1").arg(i + 1), dark.name());
        result.insert(QString("light%1").arg(i + 1), light.name());
    }

    auto relativeLuminance =
        [](const QColor& color) -> double
        {
            auto map =
                [](double v) -> double
                {
                    return v <= 0.04045
                        ? v / 12.92
                        : pow((v + 0.055) / 1.055, 2.4);
                };

            return 0.2126 * map(color.redF())
                + 0.7152 * map(color.greenF())
                + 0.0722 * map(color.blueF());
        };

    auto contrastRatio =
        [&](const QColor& a, const QColor& b)
        {
            auto l1 = relativeLuminance(a);
            auto l2 = relativeLuminance(b);
            if (l1 < l2)
                std::swap(l1, l2);

            return (l1 + 0.05) / (l2 + 0.05);
        };

    auto dark1 = QColor::fromRgb(0, 0, 0);
    auto light1 = QColor::fromRgb(255, 255, 255);
    result.insert(
        "brand_contrast",
        contrastRatio(brand_core, dark1) > contrastRatio(brand_core, light1)
            ? dark1.name()
            : light1.name());

    return result;
}

} // namespace

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
    void loadColors(
        const QString& legacyColorsFile,
        const QString& basicColorsFile,
        const QString& derivedColorsFile,
        const QString& skinColorsFile);

    void initIconCustomizations();

private:
    QJsonObject readColorDataFromFile(const QString& fileName) const;
};

void ColorTheme::Private::loadColors(
    const QString& legacyColorsFile,
    const QString& basicColorsFile,
    const QString& derivedColorsFile,
    const QString& skinColorsFile)
{
    // Load base colors and override them with the actual skin values.
    ColorThemeReader reader;

    QJsonObject originalColors = readColorDataFromFile(legacyColorsFile);
    reader.addColors(originalColors);

    const ColorTree originalColorTree = reader.getColorTree();

    QJsonObject basicColors = readColorDataFromFile(basicColorsFile);
    initializeThemeSpecificColors(&basicColors);

    reader.addColors(basicColors);

    ColorTree actualColorTree = reader.getColorTree();

    const QJsonObject generatedColors = generatedColorScheme(actualColorTree);
    reader.addColors(generatedColors);

    QJsonObject derivedColors = readColorDataFromFile(derivedColorsFile);
    reader.addColors(derivedColors);

    QJsonObject skinColors = readColorDataFromFile(skinColorsFile);
    reader.addColors(skinColors);

    actualColorTree = reader.getColorTree();

    colorTree = actualColorTree.toVariantMap();
    colorsByPath = actualColorTree.colorsByPath();

    colorSubstitutions = originalColorTree.rootColorsDelta(actualColorTree);

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
    ColorTheme(
        legacyColorsFileName(),
        basicColorsFileName(),
        derivedColorsFileName(),
        skinColorsFileName())
{
}

ColorTheme::ColorTheme(
    const QString& legacyColorsFileName,
    const QString& basicColorsFileName,
    const QString& derivedColorsFileName,
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

    d->loadColors(
        legacyColorsFileName,
        basicColorsFileName,
        derivedColorsFileName,
        skinColorsFileName);
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

bool ColorTheme::hasColor(const QString& name) const
{
    return d->colorsByPath.contains(name);
}

QColor ColorTheme::safeColor(const QString& name) const
{
    return hasColor(name) ? color(name) : QColor::fromString(name);
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

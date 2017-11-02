#include "color_theme.h"

#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QVariant>
#include <QtCore/QRegExp>
#include <QtGui/QColor>

#include <nx/utils/log/log.h>

static uint qHash(const QColor& color)
{
    return color.rgb();
}

namespace nx {
namespace client {
namespace desktop {

static const auto kSkinFileName = lit(":/skin/customization_common.json");

class ColorTheme::Private
{
public:
    QVariantMap colors;

    QHash<QString, QList<QColor>> groups;

    struct ColorInfo
    {
        QString group;
        int index;
    };
    QHash<QColor, ColorInfo> colorInfoByColor;

public:
    void loadColors();
};

void ColorTheme::Private::loadColors()
{
    QFile file(kSkinFileName);
    if (!file.open(QFile::ReadOnly))
    {
        NX_ERROR(this, lm("Cannot read skin file %1").arg(kSkinFileName));
        return;
    }

    const auto& jsonData = file.readAll();

    file.close();

    QJsonParseError error;
    const auto& json = QJsonDocument::fromJson(jsonData, &error);

    if (error.error != QJsonParseError::NoError)
    {
        NX_ERROR(this, lm("JSON parse error: %1").arg(error.errorString()));
        return;
    }

    if (!json.isObject())
    {
        NX_ERROR(this, lm("Not an object"));
        return;
    }

    const auto& globals = json.object().value(lit("globals")).toObject();
    if (globals.isEmpty())
    {
        NX_ERROR(this, lm("\"globals\" key is empty"));
        return;
    }

    QRegExp groupRegExp(lit("([^_\\d]+)[_\\d].*"));

    for (auto it = globals.begin(); it != globals.end(); ++it)
    {
        if (it->type() != QJsonValue::String)
            continue;

        const auto& colorName = it.key();
        const auto& color = QColor(it.value().toString());
        if (color.isValid())
            colors[colorName] = color;

        if (groupRegExp.exactMatch(colorName))
        {
            const auto& group = groupRegExp.cap(1);
            groups[group].append(color);
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

} // namespace desktop
} // namespace client
} // namespace nx

#include "color_theme.h"

#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QVariant>
#include <QtGui/QColor>

#include <nx/utils/log/log.h>

namespace nx {
namespace client {
namespace desktop {

static const auto skinFileName = QStringLiteral(":/skin/customization_common.json");

ColorTheme::ColorTheme(QObject* parent):
    QObject(parent)
{
    loadColors();
}

QVariantMap ColorTheme::colors() const
{
    return m_colors;
}

void ColorTheme::loadColors()
{
    QFile file(skinFileName);
    if (!file.open(QFile::ReadOnly))
    {
        NX_ERROR(this, lm("Cannot read skin file %1").arg(skinFileName));
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

    const auto& globals = json.object().value(QStringLiteral("globals")).toObject();
    if (globals.isEmpty())
    {
        NX_ERROR(this, lm("\"globals\" key is empty"));
        return;
    }

    for (auto it = globals.begin(); it != globals.end(); ++it)
    {
        if (it->type() != QJsonValue::String)
            continue;

        const auto& colorName = it.key();
        const auto& color = QColor(it.value().toString());
        if (color.isValid())
            m_colors[colorName] = color;
    }
}

} // namespace desktop
} // namespace client
} // namespace nx

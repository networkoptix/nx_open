// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "font_config.h"

#include <QtCore/QFile>
#include <QtCore/QVariant>
#include <QtGui/QGuiApplication>
#include <QtQml/QtQml>

#include <nx/reflect/instrument.h>
#include <nx/reflect/json.h>
#include <nx/utils/buffer.h>
#include <nx/utils/log/log.h>
#include <nx/utils/range_adapters.h>
#include <nx/utils/std_string_utils.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/ini.h>

template<typename Visitor>
constexpr auto nxReflectVisitAllEnumItems(QFont::Weight*, Visitor&& visitor)
{
    using Item = nx::reflect::enumeration::Item<QFont::Weight>;
    return visitor(
        Item{QFont::Thin, "thin"},
        Item{QFont::ExtraLight, "extraLight"},
        Item{QFont::Light, "light"},
        Item{QFont::Normal, "normal"},
        Item{QFont::Medium, "medium"},
        Item{QFont::DemiBold, "demiBold"},
        Item{QFont::Bold, "bold"},
        Item{QFont::ExtraBold, "extraBold"},
        Item{QFont::Black, "black"}
    );
}

namespace nx::vms::client::core {

namespace {

struct FontRecord
{
    int size = 0; //< Pixel size.
    QFont::Weight weight = QFont::Weight::Normal;
};

NX_REFLECTION_INSTRUMENT(FontRecord, (size)(weight));

using FontMap = QHash<QString, QFont>;

FontMap readFontMap(const QString& fileName, const QFont& baseFont)
{
    QFile file(fileName);
    if (!file.open(QFile::ReadOnly))
    {
        NX_ERROR(NX_SCOPE_TAG, "Can't open font config file, path %1, code: %2, error: %3",
            fileName, file.error(), file.errorString());

        return {};
    }

    auto [data, error] = nx::reflect::json::deserialize<std::map<QString, FontRecord>>(
        nx::toBufferView(file.readAll()));

    if (!error.success)
    {
        NX_ERROR(NX_SCOPE_TAG, "Can't parse font config, file: %1, error: %2",
            fileName, error.errorDescription);
    }

    FontMap result;
    for (const auto& [name, rec]: data)
    {
        if (!NX_ASSERT(rec.size > 0, "Invalid font %1 size %2", name, rec.size))
            continue;

        auto font = baseFont;
        font.setPixelSize(rec.size);

        if (NX_ASSERT(rec.weight > 0 && rec.weight <= 1000,
            "Invalid font %1 weight %2", name, rec.weight))
        {
            font.setWeight(rec.weight);
        }

        result[name] = font;
    }

    return result;
}

} // namespace

struct FontConfig::Private
{
    FontMap fontMap;
};

FontConfig::FontConfig(
    const QFont& baseFont,
    const QString& basicFontsFileName,
    const QString& overrideFontsFileName,
    QObject* parent):
    base_type(this, parent),
    d(new Private())
{
    NX_INFO(this, "Loading font config, base font family: %1, weight: %2, size %3px",
        baseFont.family(), baseFont.weight(), baseFont.pixelSize());
    NX_DEBUG(this, "Base file: %1, override file: %2", basicFontsFileName, overrideFontsFileName);

    d->fontMap = readFontMap(basicFontsFileName, baseFont);
    d->fontMap.insert(readFontMap(overrideFontsFileName, baseFont));

    const auto normalFont = font("normal");
    NX_INFO(this, "Loaded %1 fonts, default config font family: %2, weight: %3, size %4px",
        d->fontMap.size(), normalFont.family(), normalFont.weight(), normalFont.pixelSize());

    for (const auto& [key, value]: nx::utils::constKeyValueRange(d->fontMap))
        insert(key, value);

    freeze();
}

FontConfig::~FontConfig()
{
}

QFont FontConfig::font(const QString& name) const
{
    const auto it = d->fontMap.find(name);
    if (NX_ASSERT(it != d->fontMap.end(), "Font absent: %1", name))
        return *it;

    return {}; //< default font from QGuiApplication.
}

void FontConfig::registerQmlType()
{
    qmlRegisterSingletonType<FontConfig>("nx.vms.client.core", 1, 0, "FontConfig",
        [](QQmlEngine* qmlEngine, QJSEngine* /*jsEngine*/) -> QObject*
        {
            qmlEngine->setObjectOwnership(appContext()->fontConfig(), QQmlEngine::CppOwnership);
            return appContext()->fontConfig();
        });
}

} // namespace nx::vms::client::core

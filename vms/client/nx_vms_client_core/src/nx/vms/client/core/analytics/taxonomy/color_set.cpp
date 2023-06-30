// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "color_set.h"

#include <nx/analytics/taxonomy/abstract_color_type.h>

using namespace nx::analytics::taxonomy;

namespace nx::vms::client::core::analytics::taxonomy {

struct ColorSet::Private
{
    std::map<QString, QString> colorByName;
};

ColorSet::ColorSet(QObject* parent):
    QObject(parent),
    d(new Private())
{
}

ColorSet::~ColorSet()
{
    // Required here for forward-declared scoped pointer destruction.
}

std::vector<QString> ColorSet::items() const
{
    std::vector<QString> result;
    for (const auto& [colorName, _]: d->colorByName)
        result.push_back(colorName);

    return result;
}

QString ColorSet::color(const QString& item) const
{
    const auto colorIt = d->colorByName.find(item);
    return colorIt == d->colorByName.cend()
        ? QString()
        : colorIt->second;
}

void ColorSet::addColorType(nx::analytics::taxonomy::AbstractColorType* colorType)
{
    for (const QString& colorName: colorType->items())
        d->colorByName[colorName] = colorType->color(colorName);
}

} // namespace nx::vms::client::core::analytics::taxonomy

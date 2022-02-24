// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "color_type.h"

#include <nx/analytics/taxonomy/item_resolver.h>

#include <nx/analytics/taxonomy/internal_state.h>
#include <nx/analytics/taxonomy/error_handler.h>

namespace nx::analytics::taxonomy {

using namespace nx::vms::api::analytics;

AbstractColorType::Item toItem(const ColorItem& colorItem)
{
    return {colorItem.name, colorItem.rgb};
}

ColorType::ColorType(
    ColorTypeDescriptor colorTypeDescriptor,
    QObject* parent)
    :
    AbstractColorType(parent),
    m_descriptor(std::move(colorTypeDescriptor))
{
}

QString ColorType::id() const
{
    return m_descriptor.id;
}

QString ColorType::name() const
{
    return m_descriptor.name;
}

AbstractColorType* ColorType::base() const
{
    return m_base;
}

std::vector<QString> ColorType::baseItems() const
{
    std::vector<QString> result;
    if (!m_base)
        return result;

    for (const QString& baseColorName: m_descriptor.baseItems)
    {
        QString baseItem = m_base->color(baseColorName);
        if (!baseItem.isEmpty())
            result.push_back(baseItem);
    }

    return result;
}

std::vector<QString> ColorType::ownItems() const
{
    return m_cachedOwnItems;
}

std::vector<QString> ColorType::items() const
{
    // TODO: Cache all this stuff.
    std::vector<QString> result = m_descriptor.baseItems;
    for (const ColorItem& ownItem: m_descriptor.items)
        result.push_back(ownItem.name);

    return result;
}

QString ColorType::color(const QString& item) const
{
    const auto it = m_colorByName.find(item);
    if (it != m_colorByName.cend())
        return it->second;

    if (m_base && m_cachedBaseColorTypeNames.find(item) != m_cachedBaseColorTypeNames.cend())
        return m_base->color(item);

    return QString();
}

nx::vms::api::analytics::ColorTypeDescriptor ColorType::serialize() const
{
    return m_descriptor;
}

void ColorType::resolve(InternalState* inOutInternalState, ErrorHandler* errorHandler)
{
    if (m_resolved)
        return;

    if (m_descriptor.base && !m_descriptor.base->isEmpty())
    {
        m_base = inOutInternalState->getTypeById<ColorType>(*m_descriptor.base);
        if (NX_ASSERT(m_base, "Color Type %1: unable to find base (%2)",
            m_descriptor.id, *m_descriptor.base))
        {
            m_base->resolve(inOutInternalState, errorHandler);
        }
    }

    ItemResolver<ColorItem, QString>::Context context;

    context.typeId = m_descriptor.id;
    context.typeName = kColorTypeDescriptorTypeName;
    context.baseItemIds = &m_descriptor.baseItems;
    context.ownItems = &m_descriptor.items;
    if (m_base)
    {
        context.baseTypeId = m_base->id();
        context.availableBaseItemIds = m_base->items();
    }

    context.getId = [](const ColorItem& item) { return item.name; };
    context.resolveItem =
        [&](ColorItem* inOutColorItem, ErrorHandler* errorHandler)
        {
            return resolveItem(inOutColorItem, errorHandler);
        };

    ItemResolver<ColorItem, QString> itemResolver(std::move(context), errorHandler);
    itemResolver.resolve();

    for (const ColorItem& colorItem: m_descriptor.items)
    {
        m_colorByName[colorItem.name] = colorItem.rgb;
        m_cachedOwnItems.push_back(colorItem.name);
    }

    for (const QString& baseItemName: m_descriptor.baseItems)
        m_cachedBaseColorTypeNames.insert(baseItemName);

    m_resolved = true;
}

bool ColorType::resolveItem(ColorItem* inOutColorItem, ErrorHandler* errorHandler)
{
    if (!inOutColorItem->rgb.startsWith("#"))
    {
        errorHandler->handleError(
            ProcessingError{NX_FMT(
                "Color Type %1: color item %2, color code should start with '#' sign, "
                "given: %3",
                m_descriptor.id, inOutColorItem->name, inOutColorItem->rgb)});

        return false;
    }

    if (inOutColorItem->rgb.size() != 4 && inOutColorItem->rgb.size() != 7)
    {
        errorHandler->handleError(
            ProcessingError{NX_FMT(
                "ColorType %1: color item %2, "
                "color code must be either in 3 or 6 digit format, given: %3",
                m_descriptor.id, inOutColorItem->name, inOutColorItem->rgb)});

        return false;
    }

    QRegularExpression hexColorRegexp("^#([0-9a-fA-F]{3}){1,2}$");
    if (!hexColorRegexp.match(inOutColorItem->rgb).hasMatch())
    {
        errorHandler->handleError(
            ProcessingError{NX_FMT(
                "Color Type %1: color item %2, color code should be in the '#FFF' "
                "or in the '#FFFFFF' format, given: %3",
                m_descriptor.id, inOutColorItem->name, inOutColorItem->rgb)});

        return false;
    }

    return true;
}

} // namespace nx::analytics::taxonomy

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "enum_type.h"

#include <nx/analytics/taxonomy/item_resolver.h>
#include <nx/analytics/taxonomy/internal_state.h>
#include <nx/analytics/taxonomy/error_handler.h>

namespace nx::analytics::taxonomy {

EnumType::EnumType(
    nx::vms::api::analytics::EnumTypeDescriptor enumTypeDescriptor,
    QObject* parent)
    :
    AbstractEnumType(parent),
    m_descriptor(std::move(enumTypeDescriptor))
{
}

QString EnumType::id() const
{
    return m_descriptor.id;
}

QString EnumType::name() const
{
    return m_descriptor.name;
}

AbstractEnumType* EnumType::base() const
{
    return m_base;
}

std::vector<QString> EnumType::ownItems() const
{
    return m_descriptor.items;
}

std::vector<QString> EnumType::items() const
{
    // TODO: Cache all this stuff.
    std::vector<QString> result = m_descriptor.baseItems;
    result.insert(result.end(), m_descriptor.items.begin(), m_descriptor.items.end());

    return result;
}

nx::vms::api::analytics::EnumTypeDescriptor EnumType::serialize() const
{
    return m_descriptor;
}

void EnumType::resolve(InternalState* inOutInternalState, ErrorHandler* errorHandler)
{
    if (m_resolved)
        return;

    if (m_descriptor.base && !m_descriptor.base->isEmpty())
    {
        m_base = inOutInternalState->getTypeById<EnumType>(*m_descriptor.base);
        if (NX_ASSERT(m_base, "Enum Type %1: unable to find base (%2)",
            m_descriptor.id, *m_descriptor.base))
        {
            m_base->resolve(inOutInternalState, errorHandler);
        }
    }

    ItemResolver<QString, QString>::Context context;

    context.typeId = m_descriptor.id;
    context.typeName = kEnumTypeDescriptorTypeName;
    context.baseItemIds = &m_descriptor.baseItems;
    context.ownItems = &m_descriptor.items;
    if (m_base)
    {
        context.baseTypeId = m_base->id();
        context.availableBaseItemIds = m_base->items();
    }

    context.getId = [](const QString& item) { return item; };
    context.resolveItem =
        [](QString* /*inOutItem*/, ErrorHandler* /*errorHandler*/) { return true; };


    ItemResolver<QString, QString> itemResolver(std::move(context), errorHandler);
    itemResolver.resolve();

    m_resolved = true;
}

} // namespace nx::analytics::taxonomy

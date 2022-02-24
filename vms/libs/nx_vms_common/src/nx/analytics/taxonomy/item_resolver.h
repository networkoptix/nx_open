// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <set>

#include <nx/analytics/taxonomy/utils.h>
#include <nx/analytics/taxonomy/error_handler.h>

namespace nx::analytics::taxonomy {

template<typename Item, typename ItemId>
class ItemResolver
{
public:
    struct Context
    {
        using GetItemId = std::function<ItemId(const Item&)>;

        // @return true if item is valid.
        using ResolveItem = std::function<bool(Item* inOutItem, ErrorHandler* errorHandler)>;

        QString typeName;
        QString typeId;
        QString baseTypeId;
        std::vector<Item>* ownItems;
        std::vector<ItemId>* baseItemIds;
        std::vector<ItemId> availableBaseItemIds;

        GetItemId getId;
        ResolveItem resolveItem;
    };

public:
    ItemResolver(Context context, ErrorHandler* errorHandler):
        m_context(std::move(context)),
        m_errorHandler(errorHandler)
    {
    }

    void resolve()
    {
        if (!m_context.baseItemIds->empty() && m_context.baseTypeId.isEmpty())
        {
            m_errorHandler->handleError(
                ProcessingError{
                    NX_FMT("%1 %2: base item list is not empty, but there is no base type",
                        m_context.typeName, m_context.typeId)});

            *m_context.baseItemIds = std::vector<ItemId>();
        }

        std::vector<Item> ownItems;
        for (int i = 0; i < m_context.ownItems->size(); ++i)
        {
            if (resolveOwnItem(&(*m_context.ownItems)[i]))
                ownItems.push_back(std::move((*m_context.ownItems)[i]));
        }

        *m_context.ownItems = std::move(ownItems);

        if (m_context.baseItemIds->empty())
            return;

        for (const ItemId& itemId: m_context.availableBaseItemIds)
            m_availableBaseItemIds.insert(itemId);

        std::vector<ItemId> baseItemIds;
        for (int i = 0; i < m_context.baseItemIds->size(); ++i)
        {
            if (resolveBaseItemId((*m_context.baseItemIds)[i]))
                baseItemIds.push_back(std::move((*m_context.baseItemIds)[i]));
        }

        *m_context.baseItemIds = std::move(baseItemIds);
    }

private:
    bool resolveOwnItem(Item* item)
    {
        ItemId itemId = m_context.getId(*item);
        bool isDuplicate = contains(m_ownItemIds, itemId);

        m_ownItemIds.insert(itemId);

        if (isDuplicate)
        {
            m_errorHandler->handleError(
                ProcessingError{
                    NX_FMT("%1 %2: duplicate in the item list (%3)",
                        m_context.typeName, m_context.typeId, itemId) });

            return false;
        }

        return m_context.resolveItem(item, m_errorHandler);
    }

    bool resolveBaseItemId(const ItemId& itemId)
    {
        if (!contains(m_availableBaseItemIds, itemId))
        {
            m_errorHandler->handleError(
                ProcessingError{ NX_FMT("%1 %2: item (%3) is declared in the base item list "
                    "but is not available in the base %4 (%5)",
                    m_context.typeName, m_context.typeId, itemId,
                    m_context.typeName, m_context.baseTypeId) });

            return false;
        }

        if (contains(m_ownItemIds, itemId))
        {
            m_errorHandler->handleError(
                ProcessingError{ NX_FMT("%1 %2: item (%3) from the base item list "
                    "duplicates an item from the item list",
                    m_context.typeName, m_context.typeId, itemId) });

            return false;
        }

        if (contains(m_declaredBaseItemIds, itemId))
        {
            m_errorHandler->handleError(
                ProcessingError{ NX_FMT("%1 %2: duplicate in the base item list (%3)",
                    m_context.typeName, m_context.typeId, itemId) });

            return false;
        }

        m_declaredBaseItemIds.insert(itemId);
        return true;
    }

private:
    Context m_context;
    ErrorHandler* m_errorHandler = nullptr;

    std::set<ItemId> m_ownItemIds;
    std::set<ItemId> m_declaredBaseItemIds;
    std::set<ItemId> m_availableBaseItemIds;
};

} // namespace nx::analytics::taxonomy

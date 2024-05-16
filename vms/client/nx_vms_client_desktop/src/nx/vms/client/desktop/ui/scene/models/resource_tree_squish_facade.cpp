// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_tree_squish_facade.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QMetaEnum>
#include <QtCore/QModelIndex>

#include <client/client_globals.h>
#include <core/resource/resource.h>
#include <nx/vms/client/desktop/ui/scene/models/resource_tree_model_adapter.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>

namespace nx::vms::client::desktop {

ResourceTreeModelSquishFacade::ResourceTreeModelSquishFacade(ResourceTreeModelAdapter* adapter):
    QObject(adapter), m_adapter(adapter)
{
}

QString ResourceTreeModelSquishFacade::jsonModel()
{
    const auto itemDataToJson =
        [](const QModelIndex& index)
        {
            QJsonObject jsonItem;
            jsonItem["name"] = index.data(Qt::DisplayRole).toString();
            jsonItem["node_type"] =
                toString(index.data(Qn::NodeTypeRole).value<ResourceTree::NodeType>());

            const auto resourceIconKeyValue = index.data(Qn::ResourceIconKeyRole).toInt();
            jsonItem["icon"] = toString(
                QMetaEnum::fromType<QnResourceIconCache::Key>().valueToKeys(resourceIconKeyValue));

            if (jsonItem["node_type"] == toString(ResourceTree::NodeType::resource))
            {
                const auto resource = index.data(core::ResourceRole).value<QnResourcePtr>();
                const auto flags = toString(resource->flags());
                jsonItem["resource_flags"] = flags.mid(flags.indexOf('(') + 1).chopped(1);
            }
            return jsonItem;
        };

    const std::function<QJsonArray(const QModelIndex&)> subtreeToJson =
        [this, itemDataToJson, &subtreeToJson](const QModelIndex& parentIndex)
        {
            QJsonArray jsonItems;
            for (auto row = 0; row < m_adapter->rowCount(parentIndex); row++)
            {
                auto index = m_adapter->index(row, 0, parentIndex);
                QJsonObject jsonItem = itemDataToJson(index);
                if (m_adapter->hasChildren(index))
                    jsonItem["children"] = subtreeToJson(index);
                jsonItem["row"] = row;
                jsonItems.append(jsonItem);
            }
            return jsonItems;
        };

    QJsonObject jsonModel;
    jsonModel["tree"] = subtreeToJson(m_adapter->rootIndex());

    return QJsonDocument(jsonModel).toJson();
}

} // namespace nx::vms::client::desktop

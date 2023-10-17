// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "lookup_list_model.h"

#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/lookup_lists/lookup_list_manager.h>

namespace nx::vms::client::desktop {

LookupListModel::LookupListModel(QObject* parent):
    QObject(parent)
{
}

LookupListModel::LookupListModel(nx::vms::api::LookupListData data, QObject* parent):
    QObject(parent),
    m_data(std::move(data))
{
}

LookupListModel::~LookupListModel()
{
}

QList<QString> LookupListModel::attributeNames() const
{
    QList<QString> result;
    for (const auto& v: m_data.attributeNames)
        result.push_back(v);
    return result;
}

void LookupListModel::setAttributeNames(QList<QString> value)
{
    m_data.attributeNames.clear();
    for (const auto& v: value)
        m_data.attributeNames.push_back(v);
    emit attributeNamesChanged();
}

QList<int> LookupListModel::setFilter(const QString& filterText, int resultLimit) const
{
    return appContext()->currentSystemContext()->lookupListManager()->filter(
        m_data, filterText, resultLimit);
}

} // namespace nx::vms::client::desktop

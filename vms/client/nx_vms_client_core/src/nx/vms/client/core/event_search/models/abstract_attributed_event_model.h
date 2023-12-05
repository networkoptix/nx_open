// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <analytics/common/object_metadata.h>
#include <nx/vms/client/core/analytics/analytics_attribute_helper.h>
#include <nx/vms/client/core/event_search/models/abstract_async_search_list_model.h>

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API AbstractAttributedEventModel: public AbstractAsyncSearchListModel
{
    Q_OBJECT
    using base_type = AbstractAsyncSearchListModel;

public:
    using AbstractAsyncSearchListModel::AbstractAsyncSearchListModel;

    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    static QVariantList flattenAttributeList(const analytics::AttributeList& source);
};

} // namespace nx::vms::client::core

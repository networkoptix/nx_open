// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/core/event_search/models/analytics_search_list_model.h>

class QMenu;

namespace nx::vms::client::desktop {

class NX_VMS_CLIENT_DESKTOP_API AnalyticsSearchListModel: public core::AnalyticsSearchListModel
{
    Q_OBJECT
    using base_type = core::AnalyticsSearchListModel;

public:
    using base_type::base_type;

    virtual QVariant data(const QModelIndex& index, int role) const override;

signals:
    void pluginActionRequested(
        const QnUuid& engineId,
        const QString& actionTypeId,
        const nx::analytics::db::ObjectTrack& track,
        const QnVirtualCameraResourcePtr& camera) const;

private:
    QSharedPointer<QMenu> contextMenu(const nx::analytics::db::ObjectTrack& track) const;
    void addCreateNewListAction(QMenu* menu, const nx::analytics::db::ObjectTrack& track) const;
};

} // namespace nx::vms::client::desktop

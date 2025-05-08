// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSortFilterProxyModel>

#include <nx/vms/client/core/context_from_qml_handler.h>
#include <nx/vms/client/mobile/window_context_aware.h>

class QnLayoutsModel : public QSortFilterProxyModel,
    public nx::vms::client::mobile::WindowContextAware,
    public nx::vms::client::core::ContextFromQmlHandler
{
    Q_OBJECT

    using base_type = QSortFilterProxyModel;

public:
    enum class ItemType
    {
        AllCameras, //< All cameras of currently connected site.
        Layout,
        CloudLayout
    };
    Q_ENUM(ItemType)

    QnLayoutsModel(QObject* parent = nullptr);

    virtual bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private:
    virtual void onContextReady() override;
};

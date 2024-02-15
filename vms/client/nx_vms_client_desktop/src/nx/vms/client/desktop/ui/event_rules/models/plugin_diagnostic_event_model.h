// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtGui/QStandardItemModel>

#include <core/resource/resource_fwd.h>

namespace nx::vms::client::desktop::ui {

class PluginDiagnosticEventModel: public QStandardItemModel
{
    Q_OBJECT

public:
    enum DataRole
    {
        PluginIdRole = Qt::UserRole + 1
    };

    using QStandardItemModel::QStandardItemModel;

    void filterByCameras(
        nx::vms::common::AnalyticsEngineResourceList engines,
        const QnVirtualCameraResourceList& cameras);
    bool isValid() const;
};

} // namespace nx::vms::client::desktop::ui

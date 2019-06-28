#pragma once

#include <QtGui/QStandardItemModel>

#include <core/resource/resource_fwd.h>

namespace nx::vms::client::desktop {
namespace ui {

class PluginEventModel: public QStandardItemModel
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

} // namespace ui
} // namespace nx::vms::client::desktop

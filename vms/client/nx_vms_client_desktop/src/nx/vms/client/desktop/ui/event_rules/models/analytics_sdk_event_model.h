#pragma once

#include <QtGui/QStandardItemModel>

#include <core/resource/resource_fwd.h>

#include <nx/vms/event/event_parameters.h>

#include <nx/utils/uuid.h>
#include <common/common_module_aware.h>

namespace nx::vms::client::desktop {
namespace ui {

class AnalyticsSdkEventModel: public QnCommonModuleAware, public QStandardItemModel
{
public:
    enum DataRole
    {
        EventTypeIdRole = Qt::UserRole + 1,
        EngineIdRole,
        ValidEventRole,
    };

    AnalyticsSdkEventModel(QObject* parent = nullptr);
    ~AnalyticsSdkEventModel();

    void loadFromCameras(
        const QnVirtualCameraResourceList& cameras,
        const nx::vms::event::EventParameters& currentEventParameters);

    bool isValid() const;
};

} // namespace ui
} // namespace nx::vms::client::desktop

#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>
#include <core/resource/camera_resource.h>

#include <utils/common/connective.h>

class QnIOPortsViewModel;

namespace Ui { class IoModuleSettingsWidget; }

namespace nx::vms::client::desktop {

struct CameraSettingsDialogState;
class CameraSettingsDialogStore;

class IoModuleSettingsWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    IoModuleSettingsWidget(CameraSettingsDialogStore* store, QWidget* parent = nullptr);
    virtual ~IoModuleSettingsWidget() override;

private:
    void loadState(const CameraSettingsDialogState& state);

private:
    const QScopedPointer<Ui::IoModuleSettingsWidget> ui;
    QnIOPortsViewModel* const m_model = nullptr;
};

} // namespace nx::vms::client::desktop

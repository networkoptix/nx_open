#pragma once

#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>
#include <ui/workbench/workbench_context_aware.h>

#include <nx/utils/scoped_connections.h>

namespace Ui { class WearableCameraMotionWidget; }

namespace nx::vms::client::desktop {

struct CameraSettingsDialogState;
class CameraSettingsDialogStore;
class Aligner;

class WearableCameraMotionWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    explicit WearableCameraMotionWidget(QWidget* parent = nullptr);
    virtual ~WearableCameraMotionWidget() override;

    void setStore(CameraSettingsDialogStore* store);

    Aligner* aligner() const;

private:
    void loadState(const CameraSettingsDialogState& state);

private:
    const QScopedPointer<Ui::WearableCameraMotionWidget> ui;
    nx::utils::ScopedConnections m_storeConnections;
    Aligner* const m_aligner = nullptr;
};

} // namespace nx::vms::client::desktop

#pragma once

#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>
#include <ui/workbench/workbench_context_aware.h>

class QnDisconnectHelper;
namespace Ui { class WearableCameraMotionWidget; }

namespace nx {
namespace client {
namespace desktop {

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
    QScopedPointer<QnDisconnectHelper> m_storeConnections;
    Aligner* const m_aligner = nullptr;
};

} // namespace desktop
} // namespace client
} // namespace nx

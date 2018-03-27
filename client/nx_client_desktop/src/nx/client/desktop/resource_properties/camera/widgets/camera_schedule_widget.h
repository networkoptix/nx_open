#pragma once

#include <QtCore/QScopedPointer>

#include <QtWidgets/QWidget>

namespace Ui { class CameraScheduleWidget; }

namespace nx {
namespace client {
namespace desktop {

struct CameraSettingsDialogState;
class CameraSettingsDialogStore;

class CameraScheduleWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    explicit CameraScheduleWidget(
        CameraSettingsDialogStore* store,
        QWidget* parent = nullptr);
    virtual ~CameraScheduleWidget() override;

    //void overrideMotionType(Qn::MotionType motionTypeOverride = Qn::MT_Default);

    void setStore(CameraSettingsDialogStore* store);

private:
    void setupUi();

    void loadState(const CameraSettingsDialogState& state);


    enum AlertReason
    {
        CurrentParamsChange,
        ScheduleChange,
        EnabledChange
    };

    //void updateAlert(AlertReason when);

    Q_DISABLE_COPY(CameraScheduleWidget)

    QScopedPointer<Ui::CameraScheduleWidget> ui;
};

} // namespace desktop
} // namespace client
} // namespace nx

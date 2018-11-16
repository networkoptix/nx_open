#pragma once

#include <QtWidgets/QWidget>

namespace Ui { class ScheduleSettingsWidget; }

namespace nx::vms::client::desktop {

struct CameraSettingsDialogState;
class CameraSettingsDialogStore;
struct SchedulePaintFunctions;

class ScheduleSettingsWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    explicit ScheduleSettingsWidget(QWidget* parent = nullptr);
    virtual ~ScheduleSettingsWidget() override;

    void setStore(CameraSettingsDialogStore* store);

private:
    void setupUi();
    void loadState(const CameraSettingsDialogState& state);

    static QString motionOptionHint(const CameraSettingsDialogState& state);

private:
    QScopedPointer<Ui::ScheduleSettingsWidget> ui;
    QScopedPointer<SchedulePaintFunctions> paintFunctions;
};

} // namespace nx::vms::client::desktop

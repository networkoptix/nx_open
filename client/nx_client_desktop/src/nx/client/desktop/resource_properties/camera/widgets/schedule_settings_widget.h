#pragma once

#include <QtWidgets/QWidget>

namespace Ui { class ScheduleSettingsWidget; }

namespace nx {
namespace client {
namespace desktop {

struct CameraSettingsDialogState;
class CameraSettingsDialogStore;

class ScheduleSettingsWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    explicit ScheduleSettingsWidget(QWidget* parent = nullptr);
    virtual ~ScheduleSettingsWidget() override;

    void setStore(CameraSettingsDialogStore* store);

private:
    void loadState(const CameraSettingsDialogState& state);

private:
    QScopedPointer<Ui::ScheduleSettingsWidget> ui;
};

} // namespace desktop
} // namespace client
} // namespace nx

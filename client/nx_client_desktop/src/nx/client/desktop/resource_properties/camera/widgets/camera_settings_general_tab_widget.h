#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include "camera_info_widget.h"

namespace Ui { class CameraSettingsGeneralTabWidget; }

namespace nx {
namespace client {
namespace desktop {

struct CameraSettingsDialogState;
class CameraSettingsDialogStore;

class CameraSettingsGeneralTabWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    explicit CameraSettingsGeneralTabWidget(
        CameraSettingsDialogStore* store, QWidget* parent = nullptr);

    virtual ~CameraSettingsGeneralTabWidget() override;

signals:
    void actionRequested(nx::client::desktop::CameraInfoWidget::Action action);

private:
    void loadState(const CameraSettingsDialogState& state);

private:
    QScopedPointer<Ui::CameraSettingsGeneralTabWidget> ui;
};

} // namespace desktop
} // namespace client
} // namespace nx

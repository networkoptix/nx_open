#pragma once

#include <QtWidgets/QWidget>

namespace Ui {
class CameraSettingsGeneralTabWidget;
}

namespace nx {
namespace client {
namespace desktop {

class CameraSettingsGeneralTabWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    CameraSettingsGeneralTabWidget(QWidget* parent = nullptr);
    virtual ~CameraSettingsGeneralTabWidget() override;

private:
    QScopedPointer<Ui::CameraSettingsGeneralTabWidget> ui;
};

} // namespace desktop
} // namespace client
} // namespace nx

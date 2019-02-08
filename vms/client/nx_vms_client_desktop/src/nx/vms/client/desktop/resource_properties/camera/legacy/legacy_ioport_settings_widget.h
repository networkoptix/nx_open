#pragma once

#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>
#include <core/resource/camera_resource.h>

#include <utils/common/connective.h>

class QnIOPortsViewModel;

namespace Ui {
class LegacyIoPortSettingsWidget;
}

namespace nx::vms::client::desktop {

class LegacyIoPortSettingsWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    LegacyIoPortSettingsWidget(QWidget* parent = 0);
    virtual ~LegacyIoPortSettingsWidget();

    void updateFromResource(const QnVirtualCameraResourcePtr &camera);
    void submitToResource(const QnVirtualCameraResourcePtr &camera);
signals:
    void dataChanged();

private:
    QScopedPointer<Ui::LegacyIoPortSettingsWidget> ui;
    QnIOPortsViewModel* m_model;
};

} // namespace nx::vms::client::desktop

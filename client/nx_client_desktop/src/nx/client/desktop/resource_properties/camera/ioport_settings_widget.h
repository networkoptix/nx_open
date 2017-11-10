#pragma once

#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>
#include <core/resource/camera_resource.h>

#include <utils/common/connective.h>

class QnIOPortsViewModel;

namespace Ui {
class IoPortSettingsWidget;
}

namespace nx {
namespace client {
namespace desktop {

class IoPortSettingsWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    IoPortSettingsWidget(QWidget* parent = 0);
    virtual ~IoPortSettingsWidget();

    void updateFromResource(const QnVirtualCameraResourcePtr &camera);
    void submitToResource(const QnVirtualCameraResourcePtr &camera);
signals:
    void dataChanged();

private:
    QScopedPointer<Ui::IoPortSettingsWidget> ui;
    QnIOPortsViewModel* m_model;
};

} // namespace desktop
} // namespace client
} // namespace nx

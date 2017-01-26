#ifndef QN_IOPORT_SETTINGS_WIDGET_H
#define QN_IOPORT_SETTINGS_WIDGET_H

#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>
#include <core/resource/camera_resource.h>

#include <utils/common/connective.h>

class QnIOPortsViewModel;

namespace Ui {
    class QnIOPortSettingsWidget;
}

class QnIOPortSettingsWidget : public QWidget {
    Q_OBJECT

    typedef QWidget base_type;
public:
    QnIOPortSettingsWidget(QWidget* parent = 0);
    virtual ~QnIOPortSettingsWidget();

    void updateFromResource(const QnVirtualCameraResourcePtr &camera);
    void submitToResource(const QnVirtualCameraResourcePtr &camera);
signals:
    void dataChanged();

private:
    QScopedPointer<Ui::QnIOPortSettingsWidget> ui;
    QnIOPortsViewModel* m_model;
};

#endif // QN_IOPORT_SETTINGS_WIDGET_H

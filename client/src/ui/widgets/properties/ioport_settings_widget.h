#ifndef QN_IOPORT_SETTINGS_WIDGET_H
#define QN_IOPORT_SETTINGS_WIDGET_H

#include <QtWidgets/QWidget>

#include <api/api_fwd.h>
#include <core/resource/resource_fwd.h>
#include <core/resource/camera_resource.h>

#include <utils/common/connective.h>

class QnIOPortsActualModel;

namespace Ui {
    class QnIOPortSettingsWidget;
}

class QnIOPortSettingsWidget : public QWidget {
    Q_OBJECT

    typedef QWidget base_type;
public:
    QnIOPortSettingsWidget(QWidget* parent = 0);
    virtual ~QnIOPortSettingsWidget();

    const QnVirtualCameraResourcePtr &camera() const;
    void updateFromResource(const QnVirtualCameraResourcePtr &camera);
    void submitToResource(const QnVirtualCameraResourcePtr &camera);
private:
    void updateHeaderWidth();
private:
    QScopedPointer<Ui::QnIOPortSettingsWidget> ui;
    QnIOPortsActualModel* m_model;
};

#endif // QN_IOPORT_SETTINGS_WIDGET_H

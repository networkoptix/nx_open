#ifndef QN_FISHEYE_SETTINGS_WIDGET_H
#define QN_FISHEYE_SETTINGS_WIDGET_H

#include <QtGui/QWidget>

#include <core/resource/camera_resource.h>

#include <QWidget>

namespace Ui {
    class FisheyeSettingsWidget;
}

class QnFisheyeSettingsWidget : public QWidget {
    Q_OBJECT
public:
    QnFisheyeSettingsWidget(QWidget* parent = 0);
    virtual ~QnFisheyeSettingsWidget();

    void updateFromResource(QnSecurityCamResourcePtr camera);

signals:
    void dataChanged();
private slots:
private:
    QScopedPointer<Ui::FisheyeSettingsWidget> ui;
};

#endif // QN_FISHEYE_SETTINGS_WIDGET_H

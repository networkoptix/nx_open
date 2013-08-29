#ifndef QN_FISHEYE_SETTINGS_WIDGET_H
#define QN_FISHEYE_SETTINGS_WIDGET_H

#include <QtWidgets/QWidget>

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

    DewarpingParams dewarpingParams() const;
signals:
    void dataChanged();
private:
    qreal getAngle();
private slots:
    void at_dataChanged();
    void at_angleDataChanged();
private:
    QScopedPointer<Ui::FisheyeSettingsWidget> ui;
    bool m_silenseMode;
    DewarpingParams m_dewarpingParams;
};

#endif // QN_FISHEYE_SETTINGS_WIDGET_H

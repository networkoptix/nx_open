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

    DevorpingParams devorpingParams() const;
signals:
    void dataChanged();
private:
    qreal getAngle();
private slots:
    void at_dataChanged();
private:
    QScopedPointer<Ui::FisheyeSettingsWidget> ui;
    bool m_silenseMode;
    DevorpingParams m_devorpingParams;
};

#endif // QN_FISHEYE_SETTINGS_WIDGET_H

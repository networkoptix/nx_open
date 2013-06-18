#ifndef __ADVANCED_SETTINGS_WIDGET_H__
#define __ADVANCED_SETTINGS_WIDGET_H__

#include "core/resource/camera_resource.h"

#include <QWidget>

namespace Ui {
    class AdvancedSettingsWidget;
}


class QnAdvancedSettingsWidget : public QWidget
{
    Q_OBJECT
public:
    QnAdvancedSettingsWidget(QWidget* parent = 0);
    virtual ~QnAdvancedSettingsWidget();

    QnSecondaryStreamQuality secondaryStreamQuality() const;
    Qt::CheckState getCameraControl() const;

    void updateFromResource(QnSecurityCamResourcePtr camera);
    void updateFromResources(QnVirtualCameraResourceList camera);
signals:
    void dataChanged();
private slots:
    void at_ui_DataChanged();
    void at_restoreDefault();
private:
    void setKeepQualityVisible(bool value);
    void setQualitySlider(QnSecondaryStreamQuality quality);
    bool isKeepQualityVisible() const;
private:
    enum SliderQuality {Quality_Low, Quality_Medium, Quality_High};

	Q_DISABLE_COPY(QnAdvancedSettingsWidget)
    QScopedPointer<Ui::AdvancedSettingsWidget> ui;
private:
    bool m_disableUpdate;
};

#endif // __ADVANCED_SETTINGS_WIDGET_H__

#ifndef __ADVANCED_SETTINGS_WIDGET_H__
#define __ADVANCED_SETTINGS_WIDGET_H__

#include "core/resource/camera_resource.h"

namespace Ui {
    class AdvancedSettingsWidget;
}


class QnAdvancedSettingsWidget : public QWidget
{
    Q_OBJECT
public:
    QnAdvancedSettingsWidget(QWidget* parent = 0);
    virtual ~QnAdvancedSettingsWidget();

    QnSecurityCamResource::SecondaryStreamQuality secondaryStreamQuality() const;
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
    void setQualitySlider(QnSecurityCamResource::SecondaryStreamQuality quality);
private:
	Q_DISABLE_COPY(QnAdvancedSettingsWidget)
    QScopedPointer<Ui::AdvancedSettingsWidget> ui;
private:
    bool m_disableUpdate;
};

#endif // __ADVANCED_SETTINGS_WIDGET_H__

#pragma once

#include <QtWidgets/QWidget>

#include <client_core/connection_context_aware.h>

#include <common/common_globals.h>

#include <core/resource/resource_fwd.h>

namespace Ui {
    class CameraExpertSettingsWidget;
}

class QnCameraExpertSettingsWidget : public QWidget, public QnConnectionContextAware
{
    Q_OBJECT
public:
    QnCameraExpertSettingsWidget(QWidget* parent = 0);
    virtual ~QnCameraExpertSettingsWidget();

    void updateFromResources(const QnVirtualCameraResourceList &cameras);
    void submitToResources(const QnVirtualCameraResourceList &cameras);

    bool isSecondStreamEnabled() const;
    void setSecondStreamEnabled(bool value = true);
signals:
    void dataChanged();

private slots:
    void at_dataChanged();

    void at_restoreDefaultsButton_clicked();
    void at_qualitySlider_valueChanged(int value);

    void updateControlBlock();

private:
    Qn::SecondStreamQuality sliderPosToQuality(int pos) const;
    int qualityToSliderPos(Qn::SecondStreamQuality quality);

    bool isArecontCamera(const QnVirtualCameraResourcePtr &camera) const;
    bool isMdPolicyAllowedForCamera(const QnVirtualCameraResourcePtr& camera, const QString& mdPolicy) const;

    QScopedPointer<Ui::CameraExpertSettingsWidget> ui;
    bool m_updating;

    /* Flag if we can edit the quality settings (for isSecondStreamEnabled() function).  */
    bool m_qualityEditable;
};

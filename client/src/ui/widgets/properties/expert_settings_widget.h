#ifndef QN_ADVANCED_SETTINGS_WIDGET_H
#define QN_ADVANCED_SETTINGS_WIDGET_H

#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>

namespace Ui {
    class CameraExpertSettingsWidget;
}

class QnCameraExpertSettingsWidget : public QWidget {
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
private:
    Qn::SecondStreamQuality sliderPosToQuality(int pos) const;
    int qualityToSliderPos(Qn::SecondStreamQuality quality);

    bool isArecontCamera(const QnVirtualCameraResourcePtr &camera) const;

    QScopedPointer<Ui::CameraExpertSettingsWidget> ui;
    bool m_updating;

    /* Flag if we can edit the quality settings (for isSecondStreamEnabled() function).  */
    bool m_qualityEditable;
};

#endif // QN_ADVANCED_SETTINGS_WIDGET_H

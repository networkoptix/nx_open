#ifndef QN_ADVANCED_SETTINGS_WIDGET_H
#define QN_ADVANCED_SETTINGS_WIDGET_H

#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>
#include "core/resource/media_resource.h"

namespace Ui {
    class AdvancedSettingsWidget;
}

class QnAdvancedSettingsWidget : public QWidget {
    Q_OBJECT
public:
    QnAdvancedSettingsWidget(QWidget* parent = 0);
    virtual ~QnAdvancedSettingsWidget();

    void updateFromResources(const QnVirtualCameraResourceList &cameras);
    void submitToResources(const QnVirtualCameraResourceList &cameras);

signals:
    void dataChanged();
    void secondStreamDisabled();
private slots:
    void at_dataChanged();

    void at_restoreDefaultsButton_clicked();
    void at_qualitySlider_valueChanged(int value);
private:
    Qn::SecondStreamQuality sliderPosToQuality(int pos);
    int qualityToSliderPos(Qn::SecondStreamQuality quality);

    bool isArecontCamera(const QnVirtualCameraResourcePtr &camera) const;

    QScopedPointer<Ui::AdvancedSettingsWidget> ui;
    bool m_updating;
};

#endif // QN_ADVANCED_SETTINGS_WIDGET_H

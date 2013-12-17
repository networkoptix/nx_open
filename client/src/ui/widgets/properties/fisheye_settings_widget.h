#ifndef QN_FISHEYE_SETTINGS_WIDGET_H
#define QN_FISHEYE_SETTINGS_WIDGET_H

#include <QtWidgets/QWidget>

#include <core/ptz/media_dewarping_params.h>

#include <QWidget>

namespace Ui {
    class FisheyeSettingsWidget;
}

class QnFisheyeSettingsWidget : public QWidget {
    Q_OBJECT
public:
    QnFisheyeSettingsWidget(QWidget* parent = 0);
    virtual ~QnFisheyeSettingsWidget();

    void setMediaDewarpingParams(const QnMediaDewarpingParams &params);
    QnMediaDewarpingParams getMediaDewarpingParams() const;
signals:
    void dataChanged();
private slots:
    void at_dataChanged();

    void updateSliderFromSpinbox(double value);
    void updateSpinboxFromSlider(int value);
private:
    QScopedPointer<Ui::FisheyeSettingsWidget> ui;
    bool m_updating;
};

#endif // QN_FISHEYE_SETTINGS_WIDGET_H

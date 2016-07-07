#ifndef QN_FISHEYE_SETTINGS_WIDGET_H
#define QN_FISHEYE_SETTINGS_WIDGET_H

#include <QtGui/QImage>
#include <QtWidgets/QWidget>

#include <core/ptz/media_dewarping_params.h>

namespace Ui {
    class FisheyeSettingsWidget;
}

class QnImageProvider;

class QnFisheyeSettingsWidget : public QWidget{
    Q_OBJECT

    typedef QWidget base_type;
public:
    QnFisheyeSettingsWidget(QWidget* parent = 0);
    virtual ~QnFisheyeSettingsWidget();

    void updateFromParams(const QnMediaDewarpingParams &params, QnImageProvider *imageProvider);
    void submitToParams(QnMediaDewarpingParams &params);

    void loadPreview();
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

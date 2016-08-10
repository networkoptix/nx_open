#ifndef QN_FISHEYE_SETTINGS_WIDGET_H
#define QN_FISHEYE_SETTINGS_WIDGET_H

#include <QtGui/QImage>
#include <QtWidgets/QWidget>

#include <core/ptz/media_dewarping_params.h>

namespace Ui
{
    class FisheyeSettingsWidget;

} // namespace Ui

class QnImageProvider;

class QnFisheyeSettingsWidget : public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    QnFisheyeSettingsWidget(QWidget* parent = 0);
    virtual ~QnFisheyeSettingsWidget();

    void updateFromParams(const QnMediaDewarpingParams& params, QnImageProvider* imageProvider);
    void submitToParams(QnMediaDewarpingParams& params);

    void loadPreview();

signals:
    void dataChanged();

private:
    QScopedPointer<Ui::FisheyeSettingsWidget> ui;
};

#endif // QN_FISHEYE_SETTINGS_WIDGET_H

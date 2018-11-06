#pragma once

#include <QtGui/QImage>
#include <QtWidgets/QWidget>

#include <core/ptz/media_dewarping_params.h>

namespace Ui {
class FisheyeSettingsWidget;
} // namespace Ui

namespace nx::vms::client::desktop {

class ImageProvider;

class FisheyeSettingsWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    FisheyeSettingsWidget(QWidget* parent = 0);
    virtual ~FisheyeSettingsWidget();

    void updateFromParams(const QnMediaDewarpingParams& params, ImageProvider* imageProvider);
    void submitToParams(QnMediaDewarpingParams& params);

    bool isReadOnly() const;
    void setReadOnly(bool value);

signals:
    void dataChanged();

private:
    QScopedPointer<Ui::FisheyeSettingsWidget> ui;
};

} // namespace nx::vms::client::desktop

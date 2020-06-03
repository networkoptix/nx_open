#pragma once

#include <QtCore/QPointer>
#include <QtWidgets/QWidget>

#include <utils/common/connective.h>

namespace Qn { enum class FisheyeCameraProjection; }

class QTimer;
class QnFisheyeCalibrator;
namespace Ui { class FisheyeCalibrationWidget; }

namespace nx::vms::client::desktop {

class ImageProvider;

class FisheyeCalibrationWidget: public Connective<QWidget>
{
    Q_OBJECT
    using base_type = Connective<QWidget>;

public:
    explicit FisheyeCalibrationWidget(QWidget* parent = nullptr);
    virtual ~FisheyeCalibrationWidget();

    QPointF center() const;
    void setCenter(const QPointF& center);

    qreal radius() const;
    void setRadius(qreal radius);

    void setHorizontalStretch(const qreal& value);
    qreal horizontalStretch() const;

    void setImageProvider(ImageProvider *provider);
    ImageProvider* imageProvider() const;

    bool isReadOnly() const;
    void setReadOnly(bool value);

    void init();

    void autoCalibrate();

    QImage previewImage() const;

    Qn::FisheyeCameraProjection projection() const;
    void setProjection(Qn::FisheyeCameraProjection value);

    bool isGridShown() const;
    void setGridShown(bool value);

signals:
    void autoCalibrationFinished();
    void dataChanged();
    void previewImageChanged();

private:
    void updatePage();

    void at_calibrator_finished(int errorCode);
    void at_image_animationFinished();

private:
    QScopedPointer<Ui::FisheyeCalibrationWidget> ui;
    QScopedPointer<QnFisheyeCalibrator> m_calibrator;
    QPointer<ImageProvider> m_imageProvider;
    int m_lastError = 0;
};

} // namespace nx::vms::client::desktop

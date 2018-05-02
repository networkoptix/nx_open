#pragma once

#include <QtCore/QPointer>
#include <QtWidgets/QWidget>

#include <utils/common/connective.h>

namespace Ui {
class FisheyeCalibrationWidget;
}

class QTimer;
class QnFisheyeCalibrator;
class QnImageProvider;

namespace nx {
namespace client {
namespace desktop {

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

    void setImageProvider(QnImageProvider *provider);
    QnImageProvider* imageProvider() const;

    void init();

    void autoCalibrate();

signals:
    void autoCalibrationFinished();
    void dataChanged();

private:
    void updatePage();

    void at_calibrator_finished(int errorCode);
    void at_image_animationFinished();

private:
    QScopedPointer<Ui::FisheyeCalibrationWidget> ui;
    QScopedPointer<QnFisheyeCalibrator> m_calibrator;
    QPointer<QnImageProvider> m_imageProvider;

    int m_lastError;
};

} // namespace desktop
} // namespace client
} // namespace nx

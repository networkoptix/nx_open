#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <ui/animation/animated.h>
#include <ui/animation/animation_timer_listener.h>
#include <ui/graphics/items/standard/graphics_widget.h>

/*
 * Primitive class able to paint dot indicator for specified time moment.
 */
class QnBusyIndicatorPainterPrivate;
class QnBusyIndicatorPainter : protected AnimationTimerListener
{
public:
    QnBusyIndicatorPainter();
    virtual ~QnBusyIndicatorPainter();

    /** Number of dots in the indicator: */
    unsigned int dotCount() const;
    void setDotCount(unsigned int count);

    /** Pixel spacing between dots: */
    unsigned int dotSpacing() const;
    void setDotSpacing(unsigned int spacing);

    /** Dot radius: */
    qreal dotRadius() const;
    void setDotRadius(qreal radius);

    /** Size of entire indicator without any margins: */
    QSize indicatorSize() const;

    /** Minimum opacity: */
    qreal minimumOpacity() const;
    void setMinimumOpacity(qreal opacity);

    /** Next dot animation time shift, in milliseconds: */
    int dotLagMs() const;
    void setDotLagMs(int lag);

    /** Minumum opacity time, in milliseconds: */
    unsigned int downTimeMs() const;
    void setDownTimeMs(unsigned int ms);

    /** Linear transition from minimum to maximum opacity time, in milliseconds: */
    unsigned int fadeInTimeMs() const;
    void setFadeInTimeMs(unsigned int ms);

    /** Maxumum opacity time, in milliseconds: */
    unsigned int upTimeMs() const;
    void setUpTimeMs(unsigned int ms);

    /** Linear transition from maximum to minimum opacity time, in milliseconds: */
    unsigned int fadeOutTimeMs() const;
    void setFadeOutTimeMs(unsigned int ms);

    /** Full period of single dot animation, in milliseconds: */
    unsigned int totalTimeMs() const;
    void setAnimationTimesMs(unsigned int downTimeMs, unsigned int fadeInTimeMs, unsigned int upTimeMs, unsigned int fadeOutTimeMs);

    /** Paint method: */
    void paintIndicator(QPainter* painter, const QPointF& origin);

protected:
    /** Protected overridable called when indicator size changes. */
    virtual void indicatorSizeChanged();

    /** Protected overridable called when indicator needs to be redrawn. */
    virtual void updateIndicator();

    /** Overridden tick of animation timer listener: */
    virtual void tick(int deltaMs) override;

private:
    QScopedPointer<QnBusyIndicatorPainterPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QnBusyIndicatorPainter);
};

/*
 * Widget class representing animated dot indicator.
 */
class QnBusyIndicatorWidget : public QWidget, public QnBusyIndicatorPainter
{
    Q_OBJECT

    Q_PROPERTY(unsigned int dotCount        READ dotCount       WRITE setDotCount)
    Q_PROPERTY(unsigned int dotSpacing      READ dotSpacing     WRITE setDotSpacing)
    Q_PROPERTY(qreal        dotRadius       READ dotRadius      WRITE setDotRadius)
    Q_PROPERTY(QSize        indicatorSize   READ indicatorSize)
    Q_PROPERTY(qreal        minimumOpacity  READ minimumOpacity WRITE setMinimumOpacity)
    Q_PROPERTY(int          dotLagMs        READ dotLagMs       WRITE setDotLagMs)
    Q_PROPERTY(unsigned int downTimeMs      READ downTimeMs     WRITE setDownTimeMs)
    Q_PROPERTY(unsigned int fadeInTimeMs    READ fadeInTimeMs   WRITE setFadeInTimeMs)
    Q_PROPERTY(unsigned int upTimeMs        READ upTimeMs       WRITE setUpTimeMs)
    Q_PROPERTY(unsigned int fadeOutTimeMs   READ fadeOutTimeMs  WRITE setFadeOutTimeMs)
    Q_PROPERTY(unsigned int totalTimeMs     READ totalTimeMs)

public:
    QnBusyIndicatorWidget(QWidget* parent = nullptr);

    virtual QSize minimumSizeHint() const override;
    virtual QSize sizeHint() const override;

protected:
    virtual void paintEvent(QPaintEvent* event) override;
    virtual void indicatorSizeChanged() override;
    virtual void updateIndicator() override;

    QRect indicatorRect() const;
};

/*
* Graphics Widget class representing animated dot indicator.
*/
class QnBusyIndicatorGraphicsWidget : public Animated<GraphicsWidget>, public QnBusyIndicatorPainter
{
    Q_OBJECT

    Q_PROPERTY(unsigned int dotCount        READ dotCount       WRITE setDotCount)
    Q_PROPERTY(unsigned int dotSpacing      READ dotSpacing     WRITE setDotSpacing)
    Q_PROPERTY(qreal        dotRadius       READ dotRadius      WRITE setDotRadius)
    Q_PROPERTY(QSize        indicatorSize   READ indicatorSize)
    Q_PROPERTY(qreal        minimumOpacity  READ minimumOpacity WRITE setMinimumOpacity)
    Q_PROPERTY(int          dotLagMs        READ dotLagMs       WRITE setDotLagMs)
    Q_PROPERTY(unsigned int downTimeMs      READ downTimeMs     WRITE setDownTimeMs)
    Q_PROPERTY(unsigned int fadeInTimeMs    READ fadeInTimeMs   WRITE setFadeInTimeMs)
    Q_PROPERTY(unsigned int upTimeMs        READ upTimeMs       WRITE setUpTimeMs)
    Q_PROPERTY(unsigned int fadeOutTimeMs   READ fadeOutTimeMs  WRITE setFadeOutTimeMs)
    Q_PROPERTY(unsigned int totalTimeMs     READ totalTimeMs)

    using base_type = Animated<GraphicsWidget>;

public:
    QnBusyIndicatorGraphicsWidget(QGraphicsItem* parent = nullptr, Qt::WindowFlags windowFlags = 0);

    virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = nullptr) override;

protected:
    virtual QSizeF sizeHint(Qt::SizeHint which, const QSizeF& constraint = QSizeF()) const override;
    virtual void indicatorSizeChanged() override;
    virtual void updateIndicator() override;

    QRectF indicatorRect() const;
};

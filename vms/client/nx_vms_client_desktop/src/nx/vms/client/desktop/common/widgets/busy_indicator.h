// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <qt_graphics_items/graphics_widget.h>

namespace nx::vms::client::desktop {

/*
* Primitive class able to paint dot indicator with specified painter.
*/
class BusyIndicatorPrivate;
class BusyIndicator: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit BusyIndicator(QObject* parent = nullptr);
    virtual ~BusyIndicator();

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
    QSize size() const;

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
    void paint(QPainter* painter);

signals:
    void sizeChanged();
    void updated();

private:
    void tick(int deltaMs);

private:
    struct Private;
    const QScopedPointer<Private> d;
};

/*
 * Widget class representing animated dot indicator.
 */
class BusyIndicatorWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

    Q_PROPERTY(BusyIndicator* dots READ dots)
    Q_PROPERTY(QPalette::ColorRole indicatorRole READ indicatorRole  WRITE setIndicatorRole)
    Q_PROPERTY(QPalette::ColorRole borderRole READ borderRole WRITE setBorderRole)
    Q_PROPERTY(QRect indicatorRect READ indicatorRect)

public:
    explicit BusyIndicatorWidget(QWidget* parent = nullptr);

    BusyIndicator* dots() const;

    QPalette::ColorRole indicatorRole() const;
    void setIndicatorRole(QPalette::ColorRole role);

    QPalette::ColorRole borderRole() const;
    void setBorderRole(QPalette::ColorRole role);

    virtual QSize minimumSizeHint() const override;
    virtual QSize sizeHint() const override;

    QRect indicatorRect() const; /**< rect with dots */

protected:
    virtual void paintEvent(QPaintEvent* event) override;
    virtual void paint(QPainter* painter);

    void updateIndicator();

private:
    const QScopedPointer<BusyIndicator> m_indicator;
    QPalette::ColorRole m_indicatorRole;
    QPalette::ColorRole m_borderRole;
};

/*
* Graphics Widget class representing animated dot indicator.
*/
class BusyIndicatorGraphicsWidget: public GraphicsWidget
{
    Q_OBJECT
    using base_type = GraphicsWidget;

    Q_PROPERTY(BusyIndicator* dots READ dots)
    Q_PROPERTY(QColor indicatorColor READ indicatorColor WRITE setIndicatorColor)
    Q_PROPERTY(QColor borderColor READ borderColor WRITE setBorderColor)
    Q_PROPERTY(QRectF indicatorRect READ indicatorRect)

public:
    explicit BusyIndicatorGraphicsWidget(
        QGraphicsItem* parent = nullptr, Qt::WindowFlags windowFlags = {});

    BusyIndicator* dots() const;

    QColor indicatorColor() const;
    void setIndicatorColor(QColor color);

    QColor borderColor() const;
    void setBorderColor(QColor color);

    virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = nullptr) override;

    QRectF indicatorRect() const; /**< rect with dots */

protected:
    virtual QSizeF sizeHint(Qt::SizeHint which, const QSizeF& constraint = QSizeF()) const override;

    void updateIndicator();

private:
    const QScopedPointer<BusyIndicator> m_indicator;
    QColor m_indicatorColor;
    QColor m_borderColor;
};

} // namespace nx::vms::client::desktop

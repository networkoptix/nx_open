// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtQuick/QQuickItem>

namespace nx::vms::client::core {

class GridViewport;

class GridViewportAttached: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QRect geometry READ geometry WRITE setGeometry NOTIFY geometryChanged)

public:
    GridViewportAttached(QObject* parent = nullptr);

    QRect geometry() const;
    void setGeometry(const QRect& geometry);

private:
    QQuickItem* item();
    GridViewport* parentGrid();

signals:
    void geometryChanged();

private:
    QRect m_geometry;
};

struct GridViewportParameters
{
    Q_GADGET
    Q_PROPERTY(QRect bounds MEMBER bounds CONSTANT)
    Q_PROPERTY(QPointF center MEMBER center CONSTANT)
    Q_PROPERTY(qreal scale MEMBER scale CONSTANT)

public:
    QRect bounds;
    QPointF center;
    qreal scale = 1.0;

    GridViewportParameters() = default;
    GridViewportParameters(const QRect& bounds, qreal scale);

    bool operator==(const GridViewportParameters& other) const;
    bool operator!=(const GridViewportParameters& other) const
    {
        return !(*this == other);
    }
};

class GridViewport: public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(qreal cellAspectRatio READ cellAspectRatio WRITE setCellAspectRatio
        NOTIFY cellAspectRatioChanged)
    Q_PROPERTY(QPointF viewCenter READ viewCenter WRITE setViewCenter NOTIFY viewCenterChanged)
    Q_PROPERTY(qreal viewScale READ viewScale WRITE setViewScale NOTIFY viewScaleChanged)

    Q_PROPERTY(nx::vms::client::core::GridViewportParameters fitContentsParameters
        READ fitContentsParameters NOTIFY fitContentsParametersChanged)
    Q_PROPERTY(nx::vms::client::core::GridViewportParameters fitZoomedItemParameters
        READ fitZoomedItemParameters NOTIFY fitZoomedItemParametersChanged)
    Q_PROPERTY(nx::vms::client::core::GridViewportParameters defaultParameters
        READ defaultParameters NOTIFY defaultParametersChanged)

    Q_PROPERTY(QRect gridBounds READ gridBounds NOTIFY gridBoundsChanged)
    Q_PROPERTY(qreal oneCellScale READ oneCellScale NOTIFY oneCellScaleChanged)

    Q_PROPERTY(QQuickItem* zoomedItem READ zoomedItem WRITE setZoomedItem NOTIFY zoomedItemChanged)

public:
    GridViewport(QQuickItem* parent = nullptr);
    virtual ~GridViewport() override;

    qreal cellAspectRatio() const;
    void setCellAspectRatio(qreal ratio);

    QPointF viewCenter() const;
    void setViewCenter(const QPointF& center);

    qreal viewScale() const;
    void setViewScale(qreal scale);

    /**
     * @return Viewport parameters to fit contents in viewport.
     */
    GridViewportParameters fitContentsParameters() const;

    /**
     * @return Viewport parameters to fit currently zoomed item in viewport.
     */
    GridViewportParameters fitZoomedItemParameters() const;

    /**
     * @return Default parameters which fit either contents or currently zoomed item in viewport.
     */
    GridViewportParameters defaultParameters() const;

    /**
     * @return Certain scale value to fit one grid cell in the viewport.
     */
    qreal oneCellScale() const;

    /*
     * @return Bounding rect for the contents in grid units.
     */
    QRect gridBounds() const;

    /**
     * For this item GridViewport will track and provide parameters to fit this item in viewport.
     */
    QQuickItem* zoomedItem() const;
    void setZoomedItem(QQuickItem* item);

    Q_INVOKABLE QVector2D mapToViewport(const QVector2D& vector) const;
    Q_INVOKABLE QSizeF mapToViewport(const QSizeF& size) const;
    Q_INVOKABLE QPointF mapToViewport(const QPointF& point) const;
    Q_INVOKABLE QRectF mapToViewport(const QRectF& rect) const;
    Q_INVOKABLE QVector2D mapFromViewport(const QVector2D& vector) const;
    Q_INVOKABLE QSizeF mapFromViewport(const QSizeF& size) const;
    Q_INVOKABLE QPointF mapFromViewport(const QPointF& point) const;
    Q_INVOKABLE QRectF mapFromViewport(const QRectF& rect) const;

    static GridViewportAttached* qmlAttachedProperties(QObject* object);
    static void registerQmlType();

signals:
    void cellAspectRatioChanged();
    void viewCenterChanged();
    void viewScaleChanged();

    void fitContentsParametersChanged();
    void fitZoomedItemParametersChanged();
    void defaultParametersChanged();

    void oneCellScaleChanged();
    void gridBoundsChanged();

    void zoomedItemChanged();

protected:
    virtual void itemChange(ItemChange change, const ItemChangeData& value) override;
    virtual void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;

private:
    class Private;
    QScopedPointer<Private> const d;
    friend class GridViewportAttached;
};

} // namespace nx::vms::client::core

QML_DECLARE_TYPEINFO(nx::vms::client::core::GridViewport, QML_HAS_ATTACHED_PROPERTIES)

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtQuick/QQuickItem>

namespace nx::vms::client::core {
namespace positioners {

class Grid;

class GridAttached: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QRect geometry READ geometry WRITE setGeometry NOTIFY geometryChanged)

public:
    GridAttached(QObject* parent = nullptr);

    QRect geometry() const;
    void setGeometry(const QRect& geometry);

private:
    QQuickItem* item();
    Grid* parentGrid();
    void repopulateLayout();

signals:
    void geometryChanged();

private:
    QRect m_geometry;
};

class Grid: public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(QRect gridBounds READ gridBounds NOTIFY gridBoundsChanged)
    Q_PROPERTY(QRectF contentBounds READ contentBounds NOTIFY contentBoundsChanged)
    Q_PROPERTY(QSizeF cellSize READ cellSize WRITE setCellSize NOTIFY cellSizeChanged)

    using base_type = QQuickItem;

public:
    Grid(QQuickItem* parent = nullptr);
    virtual ~Grid() override;

    QRect gridBounds() const;
    QRectF contentBounds() const;

    QSizeF cellSize() const;
    void setCellSize(const QSizeF& size);

    static GridAttached* qmlAttachedProperties(QObject* object);

signals:
    void gridBoundsChanged();
    void contentBoundsChanged();
    void cellSizeChanged();

protected:
    virtual void itemChange(ItemChange change, const ItemChangeData& value) override;
    void positionItems();

private:
    void updateItemGridGeometry(QQuickItem* item, const QRect& geometry);
    void updateBounds();

private:
    class Private;
    QScopedPointer<Private> const d;
    friend class Private;
    friend class GridAttached;
};

} // namespace positioners
} // namespace nx::vms::client::core

QML_DECLARE_TYPEINFO(nx::vms::client::core::positioners::Grid, QML_HAS_ATTACHED_PROPERTIES)

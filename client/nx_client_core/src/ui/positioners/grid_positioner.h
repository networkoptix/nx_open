#pragma once

#include <QtQuick/QQuickItem>

namespace nx {
namespace client {
namespace core {
namespace ui {
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
    Q_PROPERTY(QSize gridSize READ gridSize NOTIFY gridSizeChanged)
    Q_PROPERTY(QSizeF cellSize READ cellSize WRITE setCellSize NOTIFY cellSizeChanged)

    using base_type = QQuickItem;

public:
    Grid(QQuickItem* parent = nullptr);
    virtual ~Grid() override;

    QSize gridSize() const;

    QSizeF cellSize() const;
    void setCellSize(const QSizeF& size);

    static GridAttached* qmlAttachedProperties(QObject* object);

signals:
    void gridSizeChanged();
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
} // namespace ui
} // namespace core
} // namespace client
} // namespace nx

QML_DECLARE_TYPEINFO(nx::client::core::ui::positioners::Grid, QML_HAS_ATTACHED_PROPERTIES)

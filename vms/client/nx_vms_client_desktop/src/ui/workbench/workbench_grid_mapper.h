#ifndef QN_WORKBENCH_GRID_MAPPER_H
#define QN_WORKBENCH_GRID_MAPPER_H

#include <QtCore/QObject>
#include <QtCore/QSize>
#include <QtCore/QRect>
#include <QtCore/QPoint>

/**
 * Convenience class that stores grid parameters.
 */
class QnWorkbenchGridMapper: public QObject
{
    Q_OBJECT

public:
    QnWorkbenchGridMapper(QObject *parent = nullptr);

    virtual ~QnWorkbenchGridMapper();

    /**
     * \returns                         Origin of the grid coordinate system.
     */
    const QPointF &origin() const {
        return m_origin;
    }

    /**
     * \param origin                    New origin of the grid coordinate system.
     */
    void setOrigin(const QPointF &origin);

    /**
     * \returns                         Size of a single cell.
     */
    const QSizeF &cellSize() const {
        return m_cellSize;
    }

    /**
     * \param cellSize                  New size of a single cell.
     */
    void setCellSize(const QSizeF &cellSize);

    /**
     * \param width                     New height of a single cell.
     * \param height                    New width of a single cell.
     */
    void setCellSize(qreal width, qreal height) {
        setCellSize(QSizeF(width, height));
    }

    /**
     * Sets the layout's spacing.
     *
     * \param spacing                   Spacing value.
     */
    void setSpacing(qreal spacing);

    /**
     * Sets the default vertical spacing.
     *
     * \param spacing                   Vertical spacing value.
     */
    void setVerticalSpacing(qreal spacing);

    /**
     * Sets the default horizontal spacing.
     *
     * \param spacing                   Horizontal spacing value.
     */
    void setHorizontalSpacing(qreal spacing);

    /**
     * \returns                         Spacing.
     */
    qreal spacing() const;

    /**
     * \returns                         Grid step, sum of cell size and spacing.
     */
    QSizeF step() const;

    /**
     * \param pos                       Position in scene coordinates to map to grid coordinates.
     * \returns                         Coordinate of the grid cell that the given
     *                                  position belongs to. If the position at spacing region is
     *                                  given, returns coordinate of the closest grid cell.
     */
    QPoint mapToGrid(const QPointF &pos) const;

    /**
     * \param pos                       Position in scene coordinates to map to grid coordinates.
     * \returns                         Corresponding position in grid coordinates.
     */
    QPointF mapToGridF(const QPointF &pos) const;

    /**
     * \param size                      Size in scene coordinates.
     * \returns                         Smallest size in grid cells that fits the given size.
     */
    QSize mapToGrid(const QSizeF &size) const;

    /**
     * \param size                      Size in scene coordinates.
     * \returns                         Corresponding size in grid cells.
     */
    QSizeF mapToGridF(const QSizeF &size) const;

    /**
     * \param gridPos                   Coordinate of the grid cell.
     * \returns                         Position in scene coordinates of the top left corner of the grid cell.
     */
    QPointF mapFromGrid(const QPoint &gridPos) const;

    /**
     * \param gridPos                   Coordinate of a point on a grid.
     * \return                          Position in scene coordinates of the given point.
     */
    QPointF mapFromGridF(const QPointF &gridPos) const;

    /**
     * \param gridSize                  Size in grid cells.
     * \returns                         Corresponding size in scene coordinates.
     */
    QSizeF mapFromGrid(const QSize &gridSize) const;

    /**
     * \param gridSize                  Size in grid cells.
     * \returns                         Corresponding size in scene coordinates.
     */
    QSizeF mapFromGridF(const QSizeF &gridSize) const;

    /**
     * \param rect                      Rectangle in scene coordinates to map to grid coordinates.
     * \returns                         Smallest grid rectangle that fits the given rectangle.
     */
    QRect mapToGrid(const QRectF &rect) const;

    /**
     * \param rect                      Rectangle in scene coordinates to map to grid coordinates.
     * \returns                         Corresponding grid rectangle in grid coordinates.
     */
    QRectF mapToGridF(const QRectF &rect) const;

    /**
     * \param gridRect                  Rectangle in grid cells.
     * \returns                         Corresponding rectangle in scene coordinates.
     */
    QRectF mapFromGrid(const QRect &gridRect) const;

    /**
     * \param gridRect                  Rectangle in grid cells.
     * \returns                         Corresponding rectangle in scene coordinates.
     */
    QRectF mapFromGridF(const QRectF &gridRect) const;

    /**
     * \param delta                     Directed vector, in scene coordinates.
     * \returns                         Corresponding directed vector in grid coordinates.
     */
    QPointF mapDeltaToGridF(const QPointF &delta) const;

signals:
    void originChanged();
    void cellSizeChanged();
    void spacingChanged();

private:
    QPointF m_origin;
    QSizeF m_cellSize;
    QSizeF m_spacing;
};


#endif // QN_WORKBENCH_GRID_MAPPER_H

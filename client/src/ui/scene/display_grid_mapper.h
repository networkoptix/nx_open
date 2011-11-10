#ifndef QN_DISPLAY_GRID_MAPPER_H
#define QN_DISPLAY_GRID_MAPPER_H

#include <QObject>
#include <QSize>
#include <QRect>
#include <QPoint>
#include <utils/common/scene_utility.h>

class QnDisplayGridMapper: public QObject, protected QnSceneUtility {
    Q_OBJECT;
public:
    QnDisplayGridMapper(QObject *parent = NULL);

    virtual ~QnDisplayGridMapper();

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
    void setSpacing(const QSizeF &spacing);

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
    const QSizeF &spacing() const {
        return m_spacing;
    }

    /**
     * \returns                         Vertical spacing.
     */
    qreal verticalSpacing() const {
        return m_spacing.height();
    }
    
    /**
     * \returns                         Horizontal spacing.
     */
    qreal horizontalSpacing() const {
        return m_spacing.width();
    }

    /**
     * \returns                         Grid step, sum of cell size and spacing.
     */
    QSizeF step() const {
        return m_cellSize + m_spacing;
    }

    /**
     * \param pos                       Position in scene coordinates to map to grid coordinates.
     * \returns                         Coordinate of the grid cell that the given
     *                                  position belongs to. If the position at spacing region is
     *                                  given, returns coordinate of the closest grid cell.
     */
    QPoint mapToGrid(const QPointF &pos) const;

    /**
     * \param gridPos                   Coordinate of the grid cell.
     * \returns                         Position in scene coordinates of the top left corner of the grid cell.
     */
    QPointF mapFromGrid(const QPoint &gridPos) const;

    /**
     * \param size                      Size in scene coordinates.
     * \returns                         Smallest size in grid cells that fits the given size.
     */
    QSize mapToGrid(const QSizeF &size) const;

    /**
     * \param gridSize                  Size in grid cells.
     * \returns                         Corresponding size in scene coordinates.
     */
    QSizeF mapFromGrid(const QSize &gridSize) const;

    /**
     * \param rect                      Rectangle in scene coordinates to map to grid coordinates.
     * \returns                         Smallest grid rectangle that fits the given rectangle.
     */
    QRect mapToGrid(const QRectF &rect) const;

    /**
     * \param gridRect                  Rectangle in grid cells.
     * \returns                         Corresponding rectangle in scene coordinates.
     */
    QRectF mapFromGrid(const QRect &gridRect) const;

signals:
    void originChanged(const QPointF &oldOrigin, const QPointF &newOrigin);
    void cellSizeChanged(const QSizeF &oldSize, const QSizeF &newSize);
    void spacingChanged(const QSizeF &oldSpacing, const QSizeF &newSpacing);
    
private:
    QPointF m_origin;
    QSizeF m_cellSize;
    QSizeF m_spacing;
};


#endif // QN_DISPLAY_GRID_MAPPER_H

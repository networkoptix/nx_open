#ifndef CELLLAYOUT_H
#define CELLLAYOUT_H

#include <QGraphicsLayout>
#include <QScopedPointer>
#include <QPoint>
#include <QRect>

// ### A quick workaround. Remove this once we have a better solution.
#undef override
#define override

class CellLayoutPrivate;

class CellLayout: public QGraphicsLayout
{
public:
    /**
     * Constructor.
     *
     * \param parent                    Parent graphics layout, passed to QGraphicsLayout constructor.
     */
    CellLayout(QGraphicsLayoutItem *parent = NULL);

    /**
     * Virtual destructor.
     */
    virtual ~CellLayout();

    /**
     * Note that the actual cell size may differ from the one returned by this
     * function because of the items' size constraints. 
     *
     * \returns                         Size of a single cell.
     */
    const QSizeF cellSize() const;

    /**
     * \param cellSize                  New size of a single cell.
     */
    void setCellSize(const QSizeF &cellSize);

    /**
     * \param width                     New height of a single cell.
     * \param height                    New width of a single cell.
     */
    inline void setCellSize(qreal width, qreal height);

#ifndef QN_NO_CELLLAYOUT_UNIFORM_SPACING

    /**
     * Sets the layout's default spacing, both vertical and horizontal, to spacing.
     * 
     * \param spacing                   Spacing value.
     */
    void setSpacing(qreal spacing);

    /**
     * \returns                         Layout spacing.
     */
    qreal spacing() const;

#else

    /**
     * Sets the layout's spacing.
     * 
     * \param spacing                   Spacing value.
     */
    void setSpacing(const QSizeF &spacing);

    /**
     * Sets the default vertical spacing for this layout.
     *
     * \param spacing                   Vertical spacing value.
     */
    void setVerticalSpacing(qreal spacing);

    /**
     * Sets the default horizontal spacing for this layout.
     *
     * \param spacing                   Horizontal spacing value.
     */
    void setHorizontalSpacing(qreal spacing);

    /**
     * \returns                         Layout spacing.
     */
    const QSizeF &spacing() const;

    /**
     * \returns                         Vertical spacing of this layout.
     */
    qreal verticalSpacing() const;
    
    /**
     * \returns                         Horizontal spacing of this layout.
     */
    qreal horizontalSpacing() const;

#endif

    /**
     * \param item                      Item from this layout.
     * \param alignment                 New alignment for the given item.
     */
    void setAlignment(QGraphicsLayoutItem *item, Qt::Alignment alignment);

    /**
     * \param item                      Item from this layout.
     * \returns                         Alignment of the given item.
     */
    Qt::Alignment alignment(QGraphicsLayoutItem *item) const;

    /**
     * \returns                         Bounds of this cell layout, in cells.
     */
    QRect bounds() const;

    /**
     * \returns                         Number of the first row of this cell layout.
     */
    inline int firstRow() const;

    /**
     * \returns                         Number of the last row of this cell layout.
     */
    inline int lastRow() const;

    /**
     * Note that rows cannot be enumerated by iterating through the range <tt>[0, rowCount())</tt>.
     * Use <tt>[firstRow(), lastRow())</tt> range instead.
     *
     * \returns                         Number of rows in this cell layout.
     */
    inline int rowCount() const;

    /**
     * \returns                         Number of the first column of this cell layout.
     */
    inline int firstColumn() const;

    /**
     * \returns                         Number of the last column of this cell layout.
     */
    inline int lastColumn() const;

    /**
     * Note that columns cannot be enumerated by iterating through the range <tt>[0, columnCount())</tt>.
     * Use <tt>[firstColumn(), lastColumn())</tt> range instead.
     *
     * \returns                         Number of columns in this cell layout.
     */
    inline int columnCount() const;

    /**
     * \param pos                       Position in parent coordinates to map to grid coordinates.
     * \returns                         Coordinate of the grid cell that the given
     *                                  position belongs to. If the position at spacing region is
     *                                  given, returns coordinate of the closest grid cell.
     */
    QPoint mapToGrid(const QPointF &pos) const;

    /**
     * \param gridPos                   Coordinate of the grid cell.
     * \returns                         Position in parent coordinates of the top left corner of the grid cell.
     */
    QPointF mapFromGrid(const QPoint &gridPos) const;

    /**
     * \param size                      Size on parent coordinates.
     * \returns                         Smallest size in grid cells that fits the given size.
     */
    QSize mapToGrid(const QSizeF &size) const;

    /**
     * \param gridSize                  Size in grid cells.
     * \returns                         Corresponding size in parent coordinates.
     */
    QSizeF mapFromGrid(const QSize &gridSize) const;

    /**
     * \param rect                      Rectangle in parent coordinates to map to grid coordinates.
     * \returns                         Smallest cell rectangle that fits the given rectangle.
     */
    QRect mapToGrid(const QRectF &rect) const;

    /**
     * \param gridRect                  Rectangle in grid cells.
     * \returns                         Corresponding rectangle in parent coordinates.
     */
    QRectF mapFromGrid(const QRect &gridRect) const;

    /**
     * \param column
     * \param row
     * \returns                         Item at given row and column, or NULL if no such item exist.
     */
    inline QGraphicsLayoutItem *itemAt(int row, int column) const;

    /**
     * \param cell                      Cell position. Note that x determines column, and y determines row.
     * \returns                         Item at the given cell, or NULL if no such item exist.
     */
    QGraphicsLayoutItem *itemAt(const QPoint &cell) const;

    /**
     * \param rect                      Rectangular area to examine for items, in cells.
     * \returns                         Set of items in the given area.
     */
    QSet<QGraphicsLayoutItem *> itemsAt(const QRect &rect) const;

    /**
     * \param item                      Layout item.
     * \returns                         Cell region occupied by the given item in this layout, or empty region if none.
     */
    QRect rect(QGraphicsLayoutItem *item) const;

    /**
     * Adds new item to this cell layout.
     *
     * \param item                      Item to add.
     * \param row
     * \param column
     * \param rowSpan
     * \param columnSpan
     * \param alignment                Item alignment.
     */
    inline void addItem(QGraphicsLayoutItem *item, int row, int column, int rowSpan, int columnSpan, Qt::Alignment alignment = Qt::AlignCenter);

    /**
     * Adds new item to this cell layout.
     *
     * \param item                     Item to add.
     * \param rect                     Item position and size, in cells.
     * \param alignment                Item alignment.
     */
    void addItem(QGraphicsLayoutItem *item, const QRect &rect, Qt::Alignment alignment = Qt::AlignCenter);

    virtual void setGeometry(const QRectF &rect) override;

    virtual int count() const override;

    virtual QGraphicsLayoutItem *itemAt(int index) const override;

    virtual void removeAt(int index) override;

    virtual void invalidate() override;

    /**
     * \param item                      Item to remove from this cell layout.
     */
    void removeItem(QGraphicsLayoutItem *item);

    /**
     * \param item                      Item to move to a new position.
     * \param rect                      New position.
     */
    void moveItem(QGraphicsLayoutItem *item, const QRect &rect);

protected:
    virtual QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint) const override;

protected:
    QScopedPointer<CellLayoutPrivate> d_ptr;

private:
    Q_DECLARE_PRIVATE(CellLayout);
};

int CellLayout::firstRow() const
{ return bounds().top(); }
int CellLayout::lastRow() const
{ return bounds().bottom(); }
int CellLayout::rowCount() const
{ return bounds().height(); }

int CellLayout::firstColumn() const
{ return bounds().left(); }
int CellLayout::lastColumn() const
{ return bounds().right(); }
int CellLayout::columnCount() const
{ return bounds().width(); }

inline QGraphicsLayoutItem *CellLayout::itemAt(int row, int column) const
{ return itemAt(QPoint(column, row)); }

inline void CellLayout::addItem(QGraphicsLayoutItem *item, int row, int column, int rowSpan, int columnSpan, Qt::Alignment alignment)
{ addItem(item, QRect(row, column, rowSpan, columnSpan), alignment); }

inline void CellLayout::setCellSize(qreal width, qreal height)
{ setCellSize(QSizeF(width, height)); }

#endif // CELLLAYOUT_H

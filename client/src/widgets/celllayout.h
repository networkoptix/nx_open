#ifndef CELL_LAYOUT_H
#define CELL_LAYOUT_H

#include <QGraphicsLayout>
#include <QScopedPointer>
#include <QPoint>
#include <QRect>

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
    void setCellSize(qreal width, qreal height);

    /**
     * Sets the layout's default spacing, both vertical and horizontal, to spacing.
     * 
     * \param spacing                   Spacing value.
     */
    void setSpacing(qreal spacing);

    /**
     * \returns                         Vertical spacing of this layout.
     */
    qreal verticalSpacing() const;
    
    /**
     * \returns                         Horizontal spacing of this layout.
     */
    qreal horizontalSpacing() const;

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
     * \returns                         Bounds of this cell layout, in cells.
     */
    QRect bounds() const;

    /**
     * \returns                         Number of the first row of this cell layout.
     */
    int startRow() const;

    /**
     * \returns                         Number of the row past the last row of this cell layout.
     */
    int endRow() const;

    /**
     * Note that rows cannot be enumerated by iterating through the range <tt>[0, rowCount())</tt>.
     * Use <tt>[startRow(), endRow())</tt> range instead.
     * 
     * \returns                         Number of rows in this cell layout. 
     */
    int rowCount() const;

    /**
     * \returns                         Number of the first column of this cell layout.
     */
    int startColumn() const;

    /**
     * \returns                         Number of the column past the last column of this cell layout.
     */
    int endColumn() const;

    /**
     * Note that columns cannot be enumerated by iterating through the range <tt>[0, columnCount())</tt>.
     * Use <tt>[startColumn(), endColumn())</tt> range instead.
     * 
     * \returns                         Number of columns in this cell layout. 
     */
    int columnCount() const;

    /**
     * \param column
     * \param row
     * \returns                         Item at given row and column, or NULL if no such item exist.
     */
    QGraphicsLayoutItem *itemAt(int row, int column) const;

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
    QRect itemRect(QGraphicsLayoutItem *item) const;

    /**
     * Adds new item to this cell layout. 
     *
     * \param item                      Item to add.
     * \param row
     * \param column
     * \param rowSpan
     * \param columnSpan
     */
    void addItem(QGraphicsLayoutItem *item, int row, int column, int rowSpan, int columnSpan);

    /**
     * Adds new item to this cell layout.
     * 
     * \param item                     Item to add.
     * \param rect                     Item position and size, in cells.
     */
    void addItem(QGraphicsLayoutItem *item, const QRect &rect);

    virtual void setGeometry(const QRectF &rect) override;

    virtual int count() const override;

    virtual QGraphicsLayoutItem *itemAt(int index) const override;

    virtual void removeAt(int index) override;

    virtual void invalidate() override;

    /**
     * \param item                      Item to remove from this cell layout.
     */
    void removeItem(QGraphicsLayoutItem *item);

protected:
    virtual QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint) const override;

    // TODO: move d_ptr to common base.

    QScopedPointer<CellLayoutPrivate> d_ptr; 

    Q_DECLARE_PRIVATE(CellLayout);
};


inline QGraphicsLayoutItem *CellLayout::itemAt(int row, int column) const 
{
    return itemAt(QPoint(column, row));
}

inline void CellLayout::addItem(QGraphicsLayoutItem *item, int row, int column, int rowSpan, int columnSpan)
{
    addItem(item, QRect(row, column, rowSpan, columnSpan));
}

inline void CellLayout::setCellSize(qreal width, qreal height) 
{
    setCellSize(QSizeF(width, height));
}

#endif // CELL_LAYOUT_H

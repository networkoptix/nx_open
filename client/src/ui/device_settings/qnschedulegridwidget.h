#ifndef __SCHEDULE_GRID_WIDGET_H_
#define __SCHEDULE_GRID_WIDGET_H_

#include <QtGui/QWidget>

static const int SEL_CELL_CLR_DELTA = 40;
static const int COLOR_LIGHT = 100;
static const int TEXT_SPACING = 4;
static const int COL_COUNT = 24;
static const int ROW_COUNT = 7;

class QnScheduleGridWidget : public QWidget
{
    Q_OBJECT

public:
    explicit QnScheduleGridWidget(QWidget *parent = 0);

    enum ParamType{ FirstParam, SecondParam, ColorParam, ParamType_Count };

    void setDefaultParam(ParamType number, const QVariant& value);
    void setShowFirstParam(bool value);
    void setShowSecondParam(bool value);

    inline int rowCount() const { return ROW_COUNT; }
    inline int columnCount() const { return COL_COUNT; }

    QVariant getCellParam(const QPoint &cell, ParamType paramType) const;
    void setCellParam(const QPoint &cell, ParamType paramType, const QVariant &value);

Q_SIGNALS:
    void needReadCellParams(const QPoint &cell);

protected:
    virtual void mouseMoveEvent(QMouseEvent *event);
    virtual void mousePressEvent(QMouseEvent *event);
    virtual void mouseReleaseEvent(QMouseEvent *event);
    virtual void leaveEvent(QEvent *event);
    virtual void paintEvent(QPaintEvent *event);
    virtual void resizeEvent(QResizeEvent *event);

private:
    qreal getCellSize() const;
    QPoint getCell(const QPoint &p, bool doTruncate) const;
    void initMetrics();
    void updateCellValue(const QPoint& cell);

private:
    typedef QVariant CellParams[ParamType_Count];

    CellParams m_defaultParams;
    CellParams m_gridParams[COL_COUNT][ROW_COUNT];
    bool m_showFirstParam;
    bool m_showSecondParam;
    QStringList m_weekDays;
    QVector<QSize> m_weekDaysSize;
    int m_gridLeftOffset;
    int m_gridTopOffset;
    QPoint m_mousePressPos;
    QPoint m_mouseMovePos;
    QPoint m_mouseMoveCell;
    QRect m_selectedRect;
    bool m_mousePressed;
    QFont m_labelsFont;
    QFont m_gridFont;
};

#endif // __SCHEDULE_GRID_WIDGET_H_

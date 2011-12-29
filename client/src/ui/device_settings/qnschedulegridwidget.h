#ifndef __SCHEDULE_GRID_WIDGET_H_
#define __SCHEDULE_GRID_WIDGET_H_

#include <QWidget>

static const int COL_COUNT = 24;
static const int ROW_COUNT = 7;


class QnScheduleGridWidget: public QWidget
{
    Q_OBJECT
public:
    QnScheduleGridWidget(QWidget* parent);

    enum ParamType{ParamType_Color, ParamType_First, ParamType_Second, ParamNum_Dummy};

    void setDefaultParam(ParamType number, const QVariant& value);
    void setShowFirstParam(bool value);
    void setShowSecondParam(bool value);
protected:

    virtual void mouseMoveEvent(QMouseEvent * event );
    virtual void mousePressEvent(QMouseEvent * event );
    virtual void mouseReleaseEvent(QMouseEvent * event);
    virtual void leaveEvent(QEvent * event );
    virtual void paintEvent(QPaintEvent * event);
    virtual void resizeEvent(QResizeEvent * event);
private:
    qreal getCellSize();
    QPoint getCell(const QPoint& p, bool doTruncate);
    void initMetrics();
    void updateCellValue(const QPoint& cell);
private:
    typedef QVariant CellParams[ParamNum_Dummy];

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

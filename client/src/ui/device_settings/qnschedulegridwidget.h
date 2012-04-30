#ifndef QN_SCHEDULE_GRID_WIDGET_H
#define QN_SCHEDULE_GRID_WIDGET_H

#include <QtGui/QWidget>

static const int SEL_CELL_CLR_DELTA = 40;
static const int TEXT_SPACING = 4;
static const int COL_COUNT = 24;
static const int ROW_COUNT = 7;


class QnScheduleGridWidget : public QWidget
{
    Q_OBJECT;
    Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly);

public:
    explicit QnScheduleGridWidget(QWidget *parent = 0);
    virtual ~QnScheduleGridWidget();

    enum ParamType{ FirstParam, SecondParam, ColorParam, ParamType_Count };

    void setDefaultParam(ParamType number, const QVariant& value);
    void setShowFirstParam(bool value);
    void setShowSecondParam(bool value);

    inline int rowCount() const { return ROW_COUNT; }
    inline int columnCount() const { return COL_COUNT; }

    QVariant cellValue(const QPoint &cell, ParamType paramType) const;
    void setCellValue(const QPoint &cell, ParamType paramType, const QVariant &value);
    void resetCellValues();

    virtual QSize minimumSizeHint() const;

    void setEnabled(bool val);
    bool isEnabled() const;

    bool isReadOnly() const;
    void setReadOnly(bool readOnly);

    void setMaxFps(int value);
    int getMaxFps();
signals:
    void cellActivated(const QPoint &cell);
    void cellValueChanged(const QPoint &cell);

protected:
    virtual void mouseMoveEvent(QMouseEvent *event) override;
    virtual void mousePressEvent(QMouseEvent *event) override;
    virtual void mouseReleaseEvent(QMouseEvent *event) override;
    virtual void leaveEvent(QEvent *event) override;
    virtual void paintEvent(QPaintEvent *event) override;
    virtual void resizeEvent(QResizeEvent *event) override;

private:
    typedef QVariant CellParams[ParamType_Count];

    void setCellValueInternal(const QPoint &cell, const CellParams &value);
    void updateCellValueInternal(const QPoint &cell);

    QPoint mapToGrid(const QPoint &pos, bool doTruncate) const;

    qreal cellSize() const;
    void initMetrics();

    bool isValidCell(const QPoint &cell) const;
    bool isValidRow(int row) const;
    bool isValidColumn(int column) const;

private:
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

    bool m_enabled;
    bool m_readOnly;
};

#endif // QN_SCHEDULE_GRID_WIDGET_H

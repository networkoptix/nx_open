#ifndef QN_SCHEDULE_GRID_WIDGET_H
#define QN_SCHEDULE_GRID_WIDGET_H

#include <QtCore/QVariant>
#include <QtGui/QColor>
#include <QtWidgets/QWidget>

#include <client/client_color_types.h>


//TODO: #Elric omg look at these global constants =)
static const int COL_COUNT = 24;
static const int ROW_COUNT = 7;

class QnScheduleGridWidget : public QWidget {
    Q_OBJECT
    Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly)
    Q_PROPERTY(QnScheduleGridColors colors READ colors WRITE setColors)

public:
    explicit QnScheduleGridWidget(QWidget *parent = 0);
    virtual ~QnScheduleGridWidget();

    enum ParamType {
        FpsParam,
        QualityParam,
        RecordTypeParam,
        DiffersFlagParam,
        ParamCount
    };

    void setDefaultParam(ParamType number, const QVariant& value);
    void setShowFps(bool value);
    void setShowQuality(bool value);

    inline int rowCount() const { return ROW_COUNT; }
    inline int columnCount() const { return COL_COUNT; }

    QVariant cellValue(const QPoint &cell, ParamType paramType) const;
    void setCellValue(const QPoint &cell, ParamType paramType, const QVariant &value);
    void resetCellValues();

    Qn::RecordingType cellRecordingType(const QPoint &cell) const;
    void setCellRecordingType(const QPoint &cell, const Qn::RecordingType &value);

    virtual QSize minimumSizeHint() const override;

    // TODO: #Elric implement this properly, handle ChangeEvent
    void setEnabled(bool val);
    bool isEnabled() const;

    bool isReadOnly() const;
    void setReadOnly(bool readOnly);

    void setMaxFps(int maxFps, int maxDualStreamFps); // todo: move this methods to camera schedule widget
    int getMaxFps(bool motionPlusLqOnly); // todo: move this methods to camera schedule widget

    const QnScheduleGridColors &colors() const;
    void setColors(const QnScheduleGridColors &colors);

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
    typedef QVariant CellParams[ParamCount];

    void setCellValueInternal(const QPoint &cell, const CellParams &value);
    void setCellValueInternal(const QPoint &cell, ParamType type, const QVariant &value);
    void updateCellValueInternal(const QPoint &cell);

    QPoint mapToGrid(const QPoint &pos, bool doTruncate) const;

    qreal cellSize() const;
    void initMetrics();

    bool isValidCell(const QPoint &cell) const;
    bool isValidRow(int row) const;
    bool isValidColumn(int column) const;

    QColor disabledCellColor(const QColor &baseColor) const;
private:
    CellParams m_defaultParams;
    CellParams m_gridParams[COL_COUNT][ROW_COUNT];
    bool m_showFps;
    bool m_showQuality;
    QString m_cornerText;
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

    QColor m_cellColors[Qn::RT_Count];
    QColor m_insideColors[Qn::RT_Count];
    QnScheduleGridColors m_colors;

    bool m_enabled;
    bool m_readOnly;
};

#endif // QN_SCHEDULE_GRID_WIDGET_H

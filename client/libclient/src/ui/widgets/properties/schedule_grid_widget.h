#pragma once

#include <array>
#include <QtCore/QVariant>
#include <QtGui/QColor>
#include <QtWidgets/QWidget>

#include <client/client_color_types.h>


class QnScheduleGridWidget : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly)
    Q_PROPERTY(QnScheduleGridColors colors READ colors WRITE setColors)

    static constexpr int kDaysPerWeek = 7;
    static constexpr int kHoursPerDay = 24;

public:
    explicit QnScheduleGridWidget(QWidget* parent = nullptr);
    virtual ~QnScheduleGridWidget();

    enum ParamType
    {
        FpsParam,
        QualityParam,
        RecordTypeParam,
        DiffersFlagParam,
        ParamCount
    };

    void setDefaultParam(ParamType number, const QVariant& value);
    void setShowFps(bool value);
    void setShowQuality(bool value);

    inline int rowCount() const { return kDaysPerWeek; }
    inline int columnCount() const { return kHoursPerDay; }

    QVariant cellValue(const QPoint& cell, ParamType paramType) const;
    void setCellValue(const QPoint& cell, ParamType paramType, const QVariant& value);
    void resetCellValues();

    Qn::RecordingType cellRecordingType(const QPoint& cell) const;
    void setCellRecordingType(const QPoint& cell, const Qn::RecordingType& value);

    virtual QSize minimumSizeHint() const override;

    bool isReadOnly() const;
    void setReadOnly(bool readOnly);

    void setMaxFps(int maxFps, int maxDualStreamFps); // todo: move this methods to camera schedule widget
    int getMaxFps(bool motionPlusLqOnly); // todo: move this methods to camera schedule widget

    const QnScheduleGridColors& colors() const;
    void setColors(const QnScheduleGridColors& colors);

signals:
    void cellActivated(const QPoint& cell);
    void cellValueChanged(const QPoint& cell);

    void colorsChanged();

protected:
    virtual void mouseMoveEvent(QMouseEvent* event) override;
    virtual void mousePressEvent(QMouseEvent* event) override;
    virtual void mouseReleaseEvent(QMouseEvent* event) override;
    virtual void leaveEvent(QEvent* event) override;
    virtual void paintEvent(QPaintEvent* event) override;
    virtual void resizeEvent(QResizeEvent* event) override;

private:
    using CellParams = std::array<QVariant, ParamCount>;
    using RowParams = std::array<CellParams, kDaysPerWeek>;
    using GridParams = std::array<RowParams, kHoursPerDay>;

    void setCellValueInternal(const QPoint& cell, const CellParams& value);
    void setCellValueInternal(const QPoint& cell, ParamType type, const QVariant& value);
    void updateCellValueInternal(const QPoint& cell);

    QPoint mapToGrid(const QPoint& pos, bool doTruncate) const;

    int cellSize() const;
    int calculateCellSize(bool headersCalculated) const;

    void initMetrics();

    bool isValidCell(const QPoint& cell) const;
    bool isValidRow(int row) const;
    bool isValidColumn(int column) const;

    void updateSelectedCellsRect();

    void updateCellColors();

    QRectF horizontalHeaderCell(int x) const;
    QRectF verticalHeaderCell(int y) const;
    QRectF cornerHeaderCell() const;

private:
    CellParams m_defaultParams;
    GridParams m_gridParams;
    bool m_showFps;
    bool m_showQuality;
    QString m_cornerText;
    QStringList m_weekDays;
    QSize m_cornerSize;
    int m_gridLeftOffset;
    int m_gridTopOffset;
    QPoint m_mousePressPos;
    QPoint m_mouseMovePos;
    QPoint m_mouseMoveCell;
    QPoint m_mousePressCell;
    QRect m_selectedCellsRect;
    QRect m_selectedRect;
    bool m_mousePressed;
    QFont m_labelsFont;
    QFont m_gridFont;
    QnScheduleGridColors m_colors;

    using TypeColors = std::array<QColor, Qn::RT_Count>;
    TypeColors m_cellColors;
    TypeColors m_cellColorsHovered;
    TypeColors m_insideColors;
    TypeColors m_insideColorsHovered;

    bool m_enabled;
    bool m_readOnly;

    mutable int m_cellSize;
};

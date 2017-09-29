#pragma once

#include <array>
#include <QtCore/QVariant>
#include <QtGui/QColor>
#include <QtWidgets/QWidget>

#include <common/common_globals.h>

#include <client/client_color_types.h>

#include <ui/common/custom_painted.h>


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

    void setShowFps(bool value);
    void setShowQuality(bool value);

    inline int rowCount() const { return kDaysPerWeek; }
    inline int columnCount() const { return kHoursPerDay; }

    struct CellParams
    {
        int fps;
        Qn::StreamQuality quality;
        Qn::RecordingType recordingType;
        qreal bitrateMbps; //< 0 means auto.

        CellParams();
        bool operator==(const CellParams& other) const;
    };

    CellParams brush() const;
    void setBrush(const CellParams& params);

    CellParams cellValue(const QPoint& cell) const;
    void setCellValue(const QPoint& cell, const CellParams& params);
    void resetCellValues();

    virtual QSize minimumSizeHint() const override;

    bool isReadOnly() const;
    void setReadOnly(bool readOnly);

    /** Active grid widget contains actual tasks. Widget may be set inactive e.g. when we are
     *  editing several cameras with different schedules.
     */
    bool isActive() const;
    void setActive(bool value);

    void setMaxFps(int maxFps, int maxDualStreamFps); // todo: move this methods to camera schedule widget
    int getMaxFps(bool motionPlusLqOnly); // todo: move this methods to camera schedule widget

    const QnScheduleGridColors& colors() const;
    void setColors(const QnScheduleGridColors& colors);

    CustomPaintedBase::PaintFunction paintFunction(Qn::RecordingType type) const;

signals:
    void cellActivated(const QPoint& cell);
    void cellValueChanged(const QPoint& cell);
    void cellValuesChanged();

    void colorsChanged();

protected:
    virtual void mouseMoveEvent(QMouseEvent* event) override;
    virtual void mousePressEvent(QMouseEvent* event) override;
    virtual void mouseReleaseEvent(QMouseEvent* event) override;
    virtual void leaveEvent(QEvent* event) override;
    virtual void paintEvent(QPaintEvent* event) override;
    virtual void resizeEvent(QResizeEvent* event) override;

private:
    using RowParams = std::array<CellParams, kDaysPerWeek>;
    using GridParams = std::array<RowParams, kHoursPerDay>;

    void handleCellClicked(const QPoint& cell);
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
    CellParams m_brushParams; /**< Params which we are using for user input. */
    GridParams m_gridParams;
    bool m_showFps = true;
    bool m_showQuality = true;
    QString m_cornerText;
    QStringList m_weekDays;
    QSize m_cornerSize;
    int m_gridLeftOffset = 0;
    int m_gridTopOffset = 0;
    QPoint m_mousePressPos;
    QPoint m_mouseMovePos;
    QPoint m_mouseMoveCell = QPoint(-2, -2);
    QPoint m_mousePressCell;
    QRect m_selectedCellsRect;
    QRect m_selectedRect;
    bool m_mousePressed = false;
    QFont m_labelsFont;
    QFont m_gridFont;
    QnScheduleGridColors m_colors;

    using TypeColors = std::array<QColor, Qn::RT_Count>;
    TypeColors m_cellColors;
    TypeColors m_cellColorsHovered;
    TypeColors m_insideColors;
    TypeColors m_insideColorsHovered;

    bool m_readOnly = false;
    bool m_active = true;
    bool m_trackedChanges = false;

    mutable int m_cellSize = -1;
};

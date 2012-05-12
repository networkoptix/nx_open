#ifndef QN_GLOBALS_H
#define QN_GLOBALS_H

#include <QObject>
#include <QVariant>
#include <QSizeF>

class QnGlobals: public QObject {
    Q_OBJECT;
public:
    enum Variable {
        SETTINGS_FONT,
        SHADOW_COLOR,
        SELECTION_COLOR,
        //MOTION_RUBBER_BAND_BORDER_COLOR,
        //MOTION_RUBBER_BAND_COLOR,
        //MOTION_SELECTION_COLOR,
		MOTION_MASK_COLOR,
        MOTION_MASK_RUBBER_BAND_BORDER_COLOR,
        MOTION_MASK_RUBBER_BAND_COLOR,
        FRAME_COLOR,
        SELECTED_FRAME_COLOR,

        RECORD_MOTION_COLOR,
        NO_RECORD_COLOR,
        MRS_COLOR,
        RECORD_ALWAYS_COLOR,
        OPACITY_CHANGE_PERIOD,

        SELECTION_OPACITY_DELTA,
        SELECTION_BORDER_DELTA,

        ERROR_TEXT_COLOR,

        /** Size of a single unit of workbench grid, in scene coordinates.
         * This basically is the width of a single video item in scene coordinates. */
        WORKBENCH_UNIT_SIZE,

        DEFAULT_LAYOUT_CELL_ASPECT_RATIO,

        DEFAULT_LAYOUT_CELL_SPACING,

        VARIABLE_COUNT
    };

    QnGlobals(QObject *parent = NULL);

    virtual ~QnGlobals();

    QVariant value(Variable variable);

    void setValue(Variable variable, const QVariant &value);

    static QnGlobals *instance();

#define QN_DECLARE_GLOBAL_ACCESSOR(TYPE, ACCESSOR, VARIABLE)                    \
    inline TYPE ACCESSOR() { return value(VARIABLE).value<TYPE>(); }            \
    inline void ACCESSOR(const TYPE &value) { setValue(VARIABLE, value); }      \

    QN_DECLARE_GLOBAL_ACCESSOR(QFont,   settingsFont,                   SETTINGS_FONT);
    QN_DECLARE_GLOBAL_ACCESSOR(QColor,  shadowColor,                    SHADOW_COLOR);
    QN_DECLARE_GLOBAL_ACCESSOR(QColor,  selectionColor,                 SELECTION_COLOR);
    //QN_DECLARE_GLOBAL_ACCESSOR(QColor,  motionRubberBandBorderColor,    MOTION_RUBBER_BAND_BORDER_COLOR);
    //QN_DECLARE_GLOBAL_ACCESSOR(QColor,  motionRubberBandColor,          MOTION_RUBBER_BAND_COLOR);
    //QN_DECLARE_GLOBAL_ACCESSOR(QColor,  motionSelectionColor,           MOTION_SELECTION_COLOR);
    QN_DECLARE_GLOBAL_ACCESSOR(QColor,  motionMaskRubberBandBorderColor,MOTION_MASK_RUBBER_BAND_BORDER_COLOR);
	QN_DECLARE_GLOBAL_ACCESSOR(QColor,  motionMaskRubberBandColor,      MOTION_MASK_RUBBER_BAND_COLOR);

    QN_DECLARE_GLOBAL_ACCESSOR(QColor,  motionMaskColor,                MOTION_MASK_COLOR);
	QN_DECLARE_GLOBAL_ACCESSOR(QColor,  frameColor,                     FRAME_COLOR);
    QN_DECLARE_GLOBAL_ACCESSOR(QColor,  selectedFrameColor,             SELECTED_FRAME_COLOR);
    QN_DECLARE_GLOBAL_ACCESSOR(int,     opacityChangePeriod,            OPACITY_CHANGE_PERIOD);
    QN_DECLARE_GLOBAL_ACCESSOR(QColor,  errorTextColor,                 ERROR_TEXT_COLOR);
    QN_DECLARE_GLOBAL_ACCESSOR(qreal,   workbenchUnitSize,              WORKBENCH_UNIT_SIZE);
    QN_DECLARE_GLOBAL_ACCESSOR(qreal,   defaultLayoutCellAspectRatio,   DEFAULT_LAYOUT_CELL_ASPECT_RATIO);
    QN_DECLARE_GLOBAL_ACCESSOR(QSizeF,  defaultLayoutCellSpacing,       DEFAULT_LAYOUT_CELL_SPACING);

    QN_DECLARE_GLOBAL_ACCESSOR(QColor,  recordMotionColor,              RECORD_MOTION_COLOR);
    QN_DECLARE_GLOBAL_ACCESSOR(QColor,  noRecordColor,                  NO_RECORD_COLOR);
    QN_DECLARE_GLOBAL_ACCESSOR(QColor,  mrsColor,                       MRS_COLOR);
    QN_DECLARE_GLOBAL_ACCESSOR(QColor,  recordAlwaysColor,              RECORD_ALWAYS_COLOR);

    QN_DECLARE_GLOBAL_ACCESSOR(QColor,  selectionOpacityDelta,          SELECTION_OPACITY_DELTA);
    QN_DECLARE_GLOBAL_ACCESSOR(QColor,  selectionBorderDelta,           SELECTION_BORDER_DELTA);
    

#undef QN_DECLARE_GLOBAL_ACCESSOR

private:
    QVariant m_globals[VARIABLE_COUNT];
};

#define qnGlobals (QnGlobals::instance())

#endif // QN_GLOBALS_H

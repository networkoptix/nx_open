#ifndef QN_SKIN_GLOBALS_H
#define QN_SKIN_GLOBALS_H

#include <QObject>
#include <QVariant>

class Globals: public QObject {
    Q_OBJECT;
public:
    enum Variable {
        SETTINGS_FONT,
        SHADOW_COLOR,
        SELECTION_COLOR,
        MOTION_RUBBER_BAND_BORDER_COLOR,
        MOTION_RUBBER_BAND_COLOR,
        MOTION_SELECTION_COLOR,
		MOTION_MASK_COLOR,
        MOTION_MASK_RUBBER_BAND_BORDER_COLOR,
        MOTION_MASK_RUBBER_BAND_COLOR,
        FRAME_COLOR,
        SELECTED_FRAME_COLOR,
        OPACITY_CHANGE_PERIOD,

        VARIABLE_COUNT
    };

    Globals(QObject *parent = NULL);

    virtual ~Globals();

    QVariant value(Variable variable);

    void setValue(Variable variable, const QVariant &value);

    static Globals *instance();

#define QN_DECLARE_GLOBAL_ACCESSOR(TYPE, ACCESSOR, VARIABLE)                    \
    static inline TYPE ACCESSOR() { return instance()->value(VARIABLE).value<TYPE>(); } \
    static inline void ACCESSOR(const TYPE &value) { instance()->setValue(VARIABLE, value); } \

    QN_DECLARE_GLOBAL_ACCESSOR(QFont,   settingsFont,                   SETTINGS_FONT);
    QN_DECLARE_GLOBAL_ACCESSOR(QColor,  shadowColor,                    SHADOW_COLOR);
    QN_DECLARE_GLOBAL_ACCESSOR(QColor,  selectionColor,                 SELECTION_COLOR);
    QN_DECLARE_GLOBAL_ACCESSOR(QColor,  motionRubberBandBorderColor,    MOTION_RUBBER_BAND_BORDER_COLOR);
    QN_DECLARE_GLOBAL_ACCESSOR(QColor,  motionRubberBandColor,          MOTION_RUBBER_BAND_COLOR);
    QN_DECLARE_GLOBAL_ACCESSOR(QColor,  motionSelectionColor,           MOTION_SELECTION_COLOR);
    QN_DECLARE_GLOBAL_ACCESSOR(QColor,  motionMaskRubberBandBorderColor,MOTION_MASK_RUBBER_BAND_BORDER_COLOR);
	QN_DECLARE_GLOBAL_ACCESSOR(QColor,  motionMaskRubberBandColor,      MOTION_MASK_RUBBER_BAND_COLOR);
    QN_DECLARE_GLOBAL_ACCESSOR(QColor,  motionMaskColor,                MOTION_MASK_COLOR);
	QN_DECLARE_GLOBAL_ACCESSOR(QColor,  frameColor,                     FRAME_COLOR);
    QN_DECLARE_GLOBAL_ACCESSOR(QColor,  selectedFrameColor,             SELECTED_FRAME_COLOR);
    QN_DECLARE_GLOBAL_ACCESSOR(int,     opacityChangePeriod,            OPACITY_CHANGE_PERIOD);
#undef QN_DECLARE_GLOBAL_ACCESSOR

private:
    QVariant m_globals[VARIABLE_COUNT];
};

#endif // QN_SKIN_GLOBALS_H

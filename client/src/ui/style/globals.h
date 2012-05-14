#ifndef QN_GLOBALS_H
#define QN_GLOBALS_H

#include <QObject>
#include <QVariant>
#include <QSizeF>

class QSettings;

class QnGlobals: public QObject {
    Q_OBJECT;
public:
    enum Variable {
        SETTINGS_FONT,
        SHADOW_COLOR,
        SELECTION_COLOR,
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

private:
    typedef QHash<int, QString> VariableNameHash;
    
    template<int> struct Dummy {};
    
    template<int variable>
    inline void init(const Dummy<variable> &, QSettings *settings) { init(Dummy<variable - 1>(), settings); }
    inline void init(const Dummy<-1> &, QSettings *) {}

    void initValue(Variable variable, QSettings *settings, const QString &key, const QColor &defaultValue);
    void initValue(Variable variable, QSettings *settings, const QString &key, int defaultValue);
    void initValue(Variable variable, QSettings *settings, const QString &key, qreal defaultValue);
    
    template<class T>
    void initValue(Variable variable, QSettings *, const QString &, const T &defaultValue) {
        m_globals[variable] = defaultValue;
    }

#define QN_DECLARE_GLOBAL_VARIABLE(TYPE, ACCESSOR, VARIABLE, DEFAULT_VALUE)     \
public:                                                                         \
    inline TYPE ACCESSOR() { return value(VARIABLE).value<TYPE>(); }            \
    inline void ACCESSOR(const TYPE &value) { setValue(VARIABLE, value); }      \
private:                                                                        \
    inline void init(const Dummy<VARIABLE> &, QSettings *settings) {            \
        init(Dummy<VARIABLE - 1>(), settings);                                  \
        initValue(VARIABLE, settings, QLatin1String(#ACCESSOR), DEFAULT_VALUE); \
        m_variableNames[VARIABLE] = QLatin1String(#ACCESSOR);                   \
    }

    QN_DECLARE_GLOBAL_VARIABLE(QFont,   settingsFont,                   SETTINGS_FONT,                          QFont());
    QN_DECLARE_GLOBAL_VARIABLE(QColor,  shadowColor,                    SHADOW_COLOR,                           QColor(0, 0, 0, 128));
    QN_DECLARE_GLOBAL_VARIABLE(QColor,  selectionColor,                 SELECTION_COLOR,                        QColor(0, 150, 255, 110));
    QN_DECLARE_GLOBAL_VARIABLE(QColor,  motionMaskRubberBandBorderColor,MOTION_MASK_RUBBER_BAND_BORDER_COLOR,   QColor(255, 255, 255, 80));
	QN_DECLARE_GLOBAL_VARIABLE(QColor,  motionMaskRubberBandColor,      MOTION_MASK_RUBBER_BAND_COLOR,          QColor(255, 255, 255, 40));

    QN_DECLARE_GLOBAL_VARIABLE(QColor,  motionMaskColor,                MOTION_MASK_COLOR,                      QColor(180, 180, 180, 96));
	QN_DECLARE_GLOBAL_VARIABLE(QColor,  frameColor,                     FRAME_COLOR,                            QColor(128, 128, 128, 196));
    QN_DECLARE_GLOBAL_VARIABLE(QColor,  selectedFrameColor,             SELECTED_FRAME_COLOR,                   QColor(64, 130, 180, 128));
    QN_DECLARE_GLOBAL_VARIABLE(int,     opacityChangePeriod,            OPACITY_CHANGE_PERIOD,                  250);
    QN_DECLARE_GLOBAL_VARIABLE(QColor,  errorTextColor,                 ERROR_TEXT_COLOR,                       QColor(255, 64, 64));
    QN_DECLARE_GLOBAL_VARIABLE(qreal,   workbenchUnitSize,              WORKBENCH_UNIT_SIZE,                    10000.0); /* Graphics scene has problems with handling mouse events on small scales, so the larger this number, the better. */
    QN_DECLARE_GLOBAL_VARIABLE(qreal,   defaultLayoutCellAspectRatio,   DEFAULT_LAYOUT_CELL_ASPECT_RATIO,       4.0 / 3.0);
    QN_DECLARE_GLOBAL_VARIABLE(QSizeF,  defaultLayoutCellSpacing,       DEFAULT_LAYOUT_CELL_SPACING,            QSizeF(0.1, 0.1));

    QN_DECLARE_GLOBAL_VARIABLE(QColor,  recordMotionColor,              RECORD_MOTION_COLOR,                    QColor(100, 0, 0));
    QN_DECLARE_GLOBAL_VARIABLE(QColor,  noRecordColor,                  NO_RECORD_COLOR,                        QColor(64, 64, 64));
    QN_DECLARE_GLOBAL_VARIABLE(QColor,  mrsColor,                       MRS_COLOR,                              QColor(200, 0, 0));
    QN_DECLARE_GLOBAL_VARIABLE(QColor,  recordAlwaysColor,              RECORD_ALWAYS_COLOR,                    QColor(0, 100, 0));

    QN_DECLARE_GLOBAL_VARIABLE(QColor,  selectionOpacityDelta,          SELECTION_OPACITY_DELTA,                QColor(0, 0, 0, 0x80));
    QN_DECLARE_GLOBAL_VARIABLE(QColor,  selectionBorderDelta,           SELECTION_BORDER_DELTA,                 QColor(48, 48, 48, 0));
#undef QN_DECLARE_GLOBAL_VARIABLE

private:
    QVariant m_globals[VARIABLE_COUNT];
    VariableNameHash m_variableNames;
};

#define qnGlobals (QnGlobals::instance())

#endif // QN_GLOBALS_H

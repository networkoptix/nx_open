#ifndef QN_GLOBALS_H
#define QN_GLOBALS_H

#include <QObject>
#include <QVariant>
#include <QSizeF>

#include <utils/common/property_storage.h>

typedef QVector<QColor> QnColorVector;
Q_DECLARE_METATYPE(QnColorVector)

/**
 * Global style settings.
 * 
 * Are expected to be initialized once and not to be changed afterwards. 
 * Everything that is changeable is to be implemented at application settings
 * level.
 */
class QnGlobals: public QnPropertyStorage {
    Q_OBJECT

    typedef QnPropertyStorage base_type;

public:
    enum Variable {
        SETTINGS_FONT,
        SHADOW_COLOR,
        SELECTION_COLOR,
        MOTION_MASK_COLOR,
        MOTION_MASK_RUBBER_BAND_BORDER_COLOR,
        MOTION_MASK_RUBBER_BAND_COLOR,
        MOTION_MASK_MOUSE_FRAME_COLOR,
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

        BACKGROUD_GRADIENT_COLOR,

        SYSTEM_HEALTH_COLOR_GRID,
        SYSTEM_HEALTH_COLORS,

        VARIABLE_COUNT
    };

    QnGlobals(QObject *parent = NULL);
    virtual ~QnGlobals();

    static QnGlobals *instance();


protected:
    virtual QVariant readValueFromSettings(QSettings *settings, int id, const QVariant &defaultValue) override;

private:
    QnColorVector initSystemHealthColors();

    QN_BEGIN_PROPERTY_STORAGE(VARIABLE_COUNT)
        QN_DECLARE_R_PROPERTY(QFont,    settingsFont,                   SETTINGS_FONT,                          QFont())
        QN_DECLARE_R_PROPERTY(QColor,   shadowColor,                    SHADOW_COLOR,                           QColor(0, 0, 0, 128))
        QN_DECLARE_R_PROPERTY(QColor,   selectionColor,                 SELECTION_COLOR,                        QColor(0, 150, 255, 110))
        QN_DECLARE_R_PROPERTY(QColor,   motionMaskRubberBandBorderColor,MOTION_MASK_RUBBER_BAND_BORDER_COLOR,   QColor(255, 255, 255, 80))
        QN_DECLARE_R_PROPERTY(QColor,   motionMaskRubberBandColor,      MOTION_MASK_RUBBER_BAND_COLOR,          QColor(255, 255, 255, 40))
        QN_DECLARE_R_PROPERTY(QColor,   motionMaskMouseFrameColor,      MOTION_MASK_MOUSE_FRAME_COLOR,          QColor(100, 255, 100, 127))

        QN_DECLARE_R_PROPERTY(QColor,   motionMaskColor,                MOTION_MASK_COLOR,                      QColor(180, 180, 180, 96))
        QN_DECLARE_R_PROPERTY(QColor,   frameColor,                     FRAME_COLOR,                            QColor(128, 128, 128, 196))
        QN_DECLARE_R_PROPERTY(QColor,   selectedFrameColor,             SELECTED_FRAME_COLOR,                   QColor(64, 130, 180, 128))
        QN_DECLARE_R_PROPERTY(int,      opacityChangePeriod,            OPACITY_CHANGE_PERIOD,                  250)
        QN_DECLARE_R_PROPERTY(QColor,   errorTextColor,                 ERROR_TEXT_COLOR,                       QColor(255, 64, 64))
        QN_DECLARE_R_PROPERTY(qreal,    workbenchUnitSize,              WORKBENCH_UNIT_SIZE,                    10000.0) /* Graphics scene has problems with handling mouse events on small scales, so the larger this number, the better. */
        QN_DECLARE_R_PROPERTY(qreal,    defaultLayoutCellAspectRatio,   DEFAULT_LAYOUT_CELL_ASPECT_RATIO,       16.0 / 9.0)
        QN_DECLARE_R_PROPERTY(QSizeF,   defaultLayoutCellSpacing,       DEFAULT_LAYOUT_CELL_SPACING,            QSizeF(0.1, 0.1))

        QN_DECLARE_R_PROPERTY(QColor,   recordMotionColor,              RECORD_MOTION_COLOR,                    QColor(100, 0, 0))
        QN_DECLARE_R_PROPERTY(QColor,   noRecordColor,                  NO_RECORD_COLOR,                        QColor(64, 64, 64))
        QN_DECLARE_R_PROPERTY(QColor,   mrsColor,                       MRS_COLOR,                              QColor(200, 0, 0))
        QN_DECLARE_R_PROPERTY(QColor,   recordAlwaysColor,              RECORD_ALWAYS_COLOR,                    QColor(0, 100, 0))

        QN_DECLARE_R_PROPERTY(QColor,   selectionOpacityDelta,          SELECTION_OPACITY_DELTA,                QColor(0, 0, 0, 0x80))
        QN_DECLARE_R_PROPERTY(QColor,   selectionBorderDelta,           SELECTION_BORDER_DELTA,                 QColor(48, 48, 48, 0))
        QN_DECLARE_R_PROPERTY(QColor,   backgroundGradientColor,        BACKGROUD_GRADIENT_COLOR,               QColor(5, 5, 50))

        QN_DECLARE_R_PROPERTY(QColor,   systemHealthColorGrid,          SYSTEM_HEALTH_COLOR_GRID,               QColor(66, 140, 237, 100))
        QN_DECLARE_R_PROPERTY(QnColorVector,   systemHealthColors,      SYSTEM_HEALTH_COLORS,                   initSystemHealthColors())
    QN_END_PROPERTY_STORAGE()
};

#define qnGlobals (QnGlobals::instance())

#endif // QN_GLOBALS_H

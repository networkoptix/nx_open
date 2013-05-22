#ifndef QN_GLOBALS_H
#define QN_GLOBALS_H

#include <QFont>
#include <QObject>
#include <QVariant>
#include <QSizeF>

#include <ui/style/statistics_colors.h>

#include <utils/common/property_storage.h>

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
        FRAME_COLOR,
        SELECTED_FRAME_COLOR,

        RECORD_MOTION_COLOR,
        NO_RECORD_COLOR,
        MRS_COLOR,
        RECORD_ALWAYS_COLOR,
        
        PTZ_COLOR,
        ZOOM_WINDOW_COLOR,

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

        STATISTICS_COLORS,

        POPUP_FRAME_SYSTEM,
        POPUP_FRAME_NOTIFICATION,
        POPUP_FRAME_IMPORTANT,
        POPUP_FRAME_WARNING,

        /** Maximum size of the layout background - in cells */
        LAYOUT_BACKGROUND_MAX_SIZE,

        /** Recommended area of the layout background - in square cells */
        LAYOUT_BACKGROUND_RECOMMENDED_AREA,

        /** Color of the cone that points to the raised widget origin if layout background is present. */
        RAISED_CONE_COLOR,

        /** Opacity of the cone that points to the raised widget origin if layout background is present. */
        RAISED_CONE_OPACITY,

        /** Opacity of the raised widget if layout background is present. */
        RAISED_WIDGET_OPACITY,

        VARIABLE_COUNT
    };

    QnGlobals(QObject *parent = NULL);
    virtual ~QnGlobals();

    static QnGlobals *instance();


protected:
    virtual QVariant readValueFromSettings(QSettings *settings, int id, const QVariant &defaultValue) override;
    virtual QVariant readValueFromJson(const QVariantMap &json, int id, const QVariant &defaultValue) override;
private:
    QN_BEGIN_PROPERTY_STORAGE(VARIABLE_COUNT)
        QN_DECLARE_R_PROPERTY(QFont,    settingsFont,                   SETTINGS_FONT,                          QFont())
        QN_DECLARE_R_PROPERTY(QColor,   shadowColor,                    SHADOW_COLOR,                           QColor(0, 0, 0, 128))
        QN_DECLARE_R_PROPERTY(QColor,   selectionColor,                 SELECTION_COLOR,                        QColor(0, 150, 255, 110))
        QN_DECLARE_R_PROPERTY(QColor,   motionMaskRubberBandBorderColor,MOTION_MASK_RUBBER_BAND_BORDER_COLOR,   QColor(255, 255, 255, 80))
        QN_DECLARE_R_PROPERTY(QColor,   motionMaskRubberBandColor,      MOTION_MASK_RUBBER_BAND_COLOR,          QColor(255, 255, 255, 40))

        QN_DECLARE_R_PROPERTY(QColor,   motionMaskColor,                MOTION_MASK_COLOR,                      QColor(180, 180, 180, 96))
        QN_DECLARE_R_PROPERTY(QColor,   frameColor,                     FRAME_COLOR,                            QColor(128, 128, 128, 196))
        QN_DECLARE_R_PROPERTY(QColor,   selectedFrameColor,             SELECTED_FRAME_COLOR,                   QColor(64, 130, 180, 128))
        QN_DECLARE_R_PROPERTY(QColor,   ptzColor,                       PTZ_COLOR,                              QColor(128, 196, 255, 255))
        QN_DECLARE_R_PROPERTY(QColor,   zoomWindowColor,                ZOOM_WINDOW_COLOR,                      QColor(128, 196, 255, 255))

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

        QN_DECLARE_R_PROPERTY(QnStatisticsColors,   statisticsColors,   STATISTICS_COLORS,                      QnStatisticsColors())

        QN_DECLARE_R_PROPERTY(QColor,   popupFrameSystem,               POPUP_FRAME_SYSTEM,                     QColor(255, 0, 0, 128))
        QN_DECLARE_R_PROPERTY(QColor,   popupFrameNotification,         POPUP_FRAME_NOTIFICATION,               QColor(64, 130, 180, 128))
        QN_DECLARE_R_PROPERTY(QColor,   popupFrameImportant,            POPUP_FRAME_IMPORTANT,                  QColor(255, 128, 0, 128))
        QN_DECLARE_R_PROPERTY(QColor,   popupFrameWarning,              POPUP_FRAME_WARNING,                    QColor(255, 0, 0, 128))

        QN_DECLARE_R_PROPERTY(QSize,    layoutBackgroundMaxSize,        LAYOUT_BACKGROUND_MAX_SIZE,             QSize(64, 64))
        QN_DECLARE_R_PROPERTY(int,      layoutBackgroundRecommendedArea,LAYOUT_BACKGROUND_RECOMMENDED_AREA,     40*40)


        QN_DECLARE_R_PROPERTY(QColor,   raisedConeColor,                RAISED_CONE_COLOR,                      QColor(64, 130, 180, 128))
        QN_DECLARE_R_PROPERTY(qreal,    raisedConeOpacity,              RAISED_CONE_OPACITY,                    1.0)
        QN_DECLARE_R_PROPERTY(qreal,    raisedWigdetOpacity,            RAISED_WIDGET_OPACITY,                  0.7)
    QN_END_PROPERTY_STORAGE()
};

#define qnGlobals (QnGlobals::instance())

#endif // QN_GLOBALS_H

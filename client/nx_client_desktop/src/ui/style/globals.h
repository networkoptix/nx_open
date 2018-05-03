#ifndef QN_GLOBALS_H
#define QN_GLOBALS_H

#include <QtCore/QObject>
#include <QtCore/QVariant>
#include <QtCore/QSizeF>
#include <QtGui/QFont>
#include <QtGui/QColor>

#include <utils/common/property_storage.h>

#include <nx/utils/singleton.h>

/**
 * Global style settings.
 *
 * Are expected to be initialized once and not to be changed afterwards.
 * Everything that is changeable is to be implemented at application settings
 * level.
 */
class QnGlobals: public QnPropertyStorage, public Singleton<QnGlobals> {
    Q_OBJECT
    typedef QnPropertyStorage base_type;

public:
    enum Variable {
        SETTINGS_FONT,
        MOTION_MASK_COLOR,
        MOTION_MASK_RUBBER_BAND_BORDER_COLOR,
        MOTION_MASK_RUBBER_BAND_COLOR,
        FRAME_COLOR,

        RECORD_MOTION_COLOR,
        NO_RECORD_COLOR,
        MRS_COLOR,
        RECORD_ALWAYS_COLOR,

        SELECTION_OPACITY_DELTA,
        SELECTION_BORDER_DELTA,

        ERROR_TEXT_COLOR,
        WARNING_TEXT_COLOR,
        SUCCESS_TEXT_COLOR,

        /** Size of a single unit of workbench grid, in scene coordinates.
         * This basically is the width of a single video item in scene coordinates. */
        WORKBENCH_UNIT_SIZE,



        DEFAULT_FRAME_WIDTH,
        SELECTED_FRAME_WIDTH,
        ZOOM_FRAME_WIDTH,

        DEFAULT_LAYOUT_CELL_ASPECT_RATIO,

        DEFAULT_LAYOUT_CELL_SPACING,

        BACKGROUD_GRADIENT_COLOR,

        /** Color of system notifications */
        NOTIFICATION_COLOR_SYSTEM,

        /** Color of notifications about common events */
        NOTIFICATION_COLOR_COMMON,

        /** Color of notifications about important events */
        NOTIFICATION_COLOR_IMPORTANT,

        /** Color of notifications about critical events */
        NOTIFICATION_COLOR_CRITICAL,

        /** Minimum size of the layout background - in cells */
        LAYOUT_BACKGROUND_MIN_SIZE,

        /** Maximum size of the layout background - in cells */
        LAYOUT_BACKGROUND_MAX_SIZE,

        /** Recommended area of the layout background - in square cells */
        LAYOUT_BACKGROUND_RECOMMENDED_AREA,

        /** Color of the cone that points to the raised widget origin if layout background is present. */
        RAISED_CONE_COLOR,

        /** Opacity of the raised widget if layout background is present. */
        RAISED_WIDGET_OPACITY,

        /** Background color for common progress bar. */
        PROGRESS_BAR_BACKGROUND_COLOR,

        /** Background color for a highlighted cell of a disabled business rule in the rules table. */
        BUSINESS_RULE_DISABLED_HIGHLIGHT_COLOR,

        /** Background color for an invalid business rule in the rules table. */
        BUSINESS_RULE_INVALID_BACKGROUND_COLOR,

        /** Background color for an invalid column of an invalid business rule in the rules table. */
        BUSINESS_RULE_INVALID_COLUMN_BACKGROUND_COLOR,

        /** Background color for a highlighted cell of an invalid business rule in the rules table. */
        BUSINESS_RULE_INVALID_HIGHLIGHT_COLOR,

        /** Period for button hover animation, in milliseconds. */
        BUTTON_ANIMATION_PERIOD,

        VARIABLE_COUNT
    };

    QnGlobals(QObject *parent = NULL);
    virtual ~QnGlobals();

protected:
    virtual QVariant readValueFromSettings(QSettings *settings, int id,
        const QVariant& defaultValue) const override;

    virtual QVariant readValueFromJson(const QJsonObject &json, int id,
        const QVariant& defaultValue) override;

private:
    QN_BEGIN_PROPERTY_STORAGE(VARIABLE_COUNT)
        QN_DECLARE_R_PROPERTY(QColor,   motionMaskRubberBandBorderColor,MOTION_MASK_RUBBER_BAND_BORDER_COLOR,   QColor(255, 255, 255, 80))
        QN_DECLARE_R_PROPERTY(QColor,   motionMaskRubberBandColor,      MOTION_MASK_RUBBER_BAND_COLOR,          QColor(255, 255, 255, 40))

        QN_DECLARE_R_PROPERTY(QColor,   motionMaskColor,                MOTION_MASK_COLOR,                      QColor(180, 180, 180, 96))
        QN_DECLARE_R_PROPERTY(QColor,   frameColor,                     FRAME_COLOR,                            QColor(128, 128, 128, 196))

        QN_DECLARE_R_PROPERTY(int,      buttonAnimationPeriod,          BUTTON_ANIMATION_PERIOD,                1)

        QN_DECLARE_R_PROPERTY(QColor,   errorTextColor,                 ERROR_TEXT_COLOR,                       QColor(255, 64, 64))
        QN_DECLARE_R_PROPERTY(QColor,   warningTextColor,               WARNING_TEXT_COLOR,                     QColor(255, 255, 64))
        QN_DECLARE_R_PROPERTY(QColor,   successTextColor,               SUCCESS_TEXT_COLOR,                     QColor(64, 255, 64))

        QN_DECLARE_R_PROPERTY(qreal,    workbenchUnitSize,              WORKBENCH_UNIT_SIZE,                    10000.0) /**< Graphics scene has problems with handling mouse events on small scales, so the larger this number, the better. */

        QN_DECLARE_R_PROPERTY(float,    defaultLayoutCellAspectRatio,   DEFAULT_LAYOUT_CELL_ASPECT_RATIO,       16.0f / 9.0f)
        QN_DECLARE_R_PROPERTY(qreal,    defaultLayoutCellSpacing,       DEFAULT_LAYOUT_CELL_SPACING,            0.05)

        QN_DECLARE_R_PROPERTY(QColor,   mrsColor,                       MRS_COLOR,                              QColor(200, 0, 0))

        QN_DECLARE_R_PROPERTY(QColor,   selectionOpacityDelta,          SELECTION_OPACITY_DELTA,                QColor(0, 0, 0, 0x80))
        QN_DECLARE_R_PROPERTY(QColor,   selectionBorderDelta,           SELECTION_BORDER_DELTA,                 QColor(48, 48, 48, 0))

        QN_DECLARE_R_PROPERTY(QColor,   notificationColorCommon,        NOTIFICATION_COLOR_COMMON,              QColor(103, 237, 66))
        QN_DECLARE_R_PROPERTY(QColor,   notificationColorImportant,     NOTIFICATION_COLOR_IMPORTANT,           QColor(237, 200, 66))
        QN_DECLARE_R_PROPERTY(QColor,   notificationColorCritical,      NOTIFICATION_COLOR_CRITICAL,            QColor(255, 131, 48))

        QN_DECLARE_R_PROPERTY(QSize,    layoutBackgroundMinSize,        LAYOUT_BACKGROUND_MIN_SIZE,             QSize(5, 5))
        QN_DECLARE_R_PROPERTY(QSize,    layoutBackgroundMaxSize,        LAYOUT_BACKGROUND_MAX_SIZE,             QSize(64, 64))
        QN_DECLARE_R_PROPERTY(int,      layoutBackgroundRecommendedArea,LAYOUT_BACKGROUND_RECOMMENDED_AREA,     40*40)

        QN_DECLARE_R_PROPERTY(QColor,   raisedConeColor,                RAISED_CONE_COLOR,                      QColor(64, 130, 180, 128))
        QN_DECLARE_R_PROPERTY(qreal,    raisedWigdetOpacity,            RAISED_WIDGET_OPACITY,                  0.95)

        QN_DECLARE_R_PROPERTY(QColor,   progressBarBackgroundColor,     PROGRESS_BAR_BACKGROUND_COLOR,          QColor(4, 154, 116))

        QN_DECLARE_R_PROPERTY(QColor,   businessRuleDisabledHighlightColor,         BUSINESS_RULE_DISABLED_HIGHLIGHT_COLOR,         QColor(64, 64, 64))
        QN_DECLARE_R_PROPERTY(QColor,   businessRuleInvalidBackgroundColor,         BUSINESS_RULE_INVALID_BACKGROUND_COLOR,         QColor(150, 0, 0))
        QN_DECLARE_R_PROPERTY(QColor,   businessRuleInvalidColumnBackgroundColor,   BUSINESS_RULE_INVALID_COLUMN_BACKGROUND_COLOR,  QColor(204, 0, 0))
        QN_DECLARE_R_PROPERTY(QColor,   businessRuleInvalidHighlightColor,          BUSINESS_RULE_INVALID_HIGHLIGHT_COLOR,          QColor(220, 0, 0))
    QN_END_PROPERTY_STORAGE()
};

#define qnGlobals (QnGlobals::instance())

#endif // QN_GLOBALS_H

#ifndef QN_CLIENT_COLOR_TYPES_H
#define QN_CLIENT_COLOR_TYPES_H

#include <QtCore/QMetaType>
#include <QtGui/QColor>

#include <utils/common/model_functions_fwd.h>

#include <vector>

struct QnTimeSliderColors {
public:
    QnTimeSliderColors();

    QColor positionMarker;
    QColor indicator;

    QColor selection;
    QColor selectionMarker;

    QColor pastBackground;
    QColor futureBackground;

    QColor pastRecording;
    QColor futureRecording;

    QColor pastMotion;
    QColor futureMotion;

    QColor pastBookmark;
    QColor futureBookmark;
    QColor pastBookmarkBound;
    QColor futureBookmarkBound;

    QColor separator;

    std::vector<QColor> dateBarBackgrounds;
    QColor dateBarText;

    QColor pastLastMinuteBackground;
    QColor futureLastMinuteBackground;
    QColor pastLastMinuteStripe;
    QColor futureLastMinuteStripe;

    std::vector<QColor> tickmarkLines;
    std::vector<QColor> tickmarkText;
};
#define QnTimeSliderColors_Fields (positionMarker)(indicator)(selection)(selectionMarker)\
    (pastBackground)(futureBackground)(pastRecording)(futureRecording)(pastMotion)(futureMotion)(separator)\
    (dateBarBackgrounds)(dateBarText)(pastBookmark)(futureBookmark)(pastBookmarkBound)(futureBookmarkBound)\
    (pastLastMinuteBackground)(futureLastMinuteBackground)(pastLastMinuteStripe)(futureLastMinuteStripe)\
    (tickmarkLines)(tickmarkText)

struct QnTimeScrollBarColors {
    QnTimeScrollBarColors();

    QColor indicator;
    QColor border;
    QColor handle;
};
#define QnTimeScrollBarColors_Fields (indicator)(border)(handle)


struct QnBackgroundColors {
    QnBackgroundColors();

    QColor normal;
    QColor panic;
};
#define QnBackgroundColors_Fields (normal)(panic)


struct QnCalendarColors {
    QnCalendarColors();

    QColor selection;
    QColor primaryRecording;
    QColor secondaryRecording;
    QColor primaryBookmark;
    QColor secondaryBookmark;
    QColor primaryMotion;
    QColor secondaryMotion;
    QColor separator;

    /* These are defined in calendar_item_delegate.cpp. */
    QColor getBackground(int fillType
        , bool isPrimary) const;
    QColor getMotionBackground(bool isPrimary) const;
};
#define QnCalendarColors_Fields (selection)(primaryRecording)(secondaryRecording)\
    (primaryBookmark)(secondaryBookmark)(primaryMotion)(secondaryMotion)(separator)


struct QnStatisticsColors {
    QnStatisticsColors();

    QColor grid;
    QColor frame;
    QColor cpu;
    QColor ram;
    QVector<QColor> hdds;
    QVector<QColor> network;
};
#define QnStatisticsColors_Fields (grid)(frame)(cpu)(ram)(hdds)(network)


struct QnIoModuleColors {
    QnIoModuleColors();

    QColor idLabel;
    QColor buttonBackground;
};
#define QnIoModuleColors_Fields (idLabel)(buttonBackground)


struct QnScheduleGridColors {
public:
    QnScheduleGridColors();

    QColor normalLabel;
    QColor weekendLabel;
    QColor selectedLabel;
    QColor disabledLabel;

    QColor recordNever;
    QColor recordAlways;
    QColor recordMotion;
};
#define QnScheduleGridColors_Fields (normalLabel)(weekendLabel)(selectedLabel)(disabledLabel)(recordNever)(recordAlways)(recordMotion)


struct QnGridColors {
    QnGridColors();

    QColor grid;
    QColor allowed;
    QColor disallowed;
};
#define QnGridColors_Fields (grid)(allowed)(disallowed)


struct QnPtzManageModelColors {
    QnPtzManageModelColors();

    QColor title;
    QColor invalid;
    QColor warning;
};
#define QnPtzManageModelColors_Fields (title)(invalid)(warning)


struct QnHistogramColors {
    QnHistogramColors();

    QColor background;
    QColor border;
    QColor histogram;
    QColor selection;
    QColor grid;
    QColor text;
};
#define QnHistogramColors_Fields (background)(border)(histogram)(selection)(grid)(text)


struct QnResourceWidgetFrameColors {
    QnResourceWidgetFrameColors();

    QColor normal;
    QColor active;
    QColor selected;
};
#define QnResourceWidgetFrameColors_Fields (normal)(active)(selected)

struct QnBookmarkColors {
    QnBookmarkColors();

    QColor tooltipBackground;
    QColor background;
    QColor text;

    QColor buttonsSeparator;

    QColor bookmarksSeparatorTop;
    QColor bookmarksSeparatorBottom;

    QColor tagBgNormal;
    QColor tagBgHovered;

    QColor tagTextNormal;
    QColor tagTextHovered;

    QColor moreItemsText;
};

#define QnBookmarkColors_Fields (tooltipBackground)(background)(text)   \
    (buttonsSeparator)(bookmarksSeparatorTop)(bookmarksSeparatorBottom) \
    (tagBgNormal)(tagBgHovered)(tagTextNormal)(tagTextHovered)(moreItemsText)

struct QnCompositeTextOverlayColors
{
    QnCompositeTextOverlayColors();

    QnBookmarkColors bookmarkColors;

    QColor textOverlayItemColor;

};
#define QnCompositeTextOverlayColors_Fields (bookmarkColors)(textOverlayItemColor)


struct QnLicensesListModelColors {
    QnLicensesListModelColors();

    QColor normal;
    QColor warning;
    QColor expired;
};
#define QnLicensesListModelColors_Fields (normal)(warning)(expired)

struct QnVideowallManageWidgetColors {
    QnVideowallManageWidgetColors();

    QColor desktop;
    QColor freeSpace;
    QColor item;
    QColor text;
    QColor error;
};
#define QnVideowallManageWidgetColors_Fields (desktop)(freeSpace)(item)(text)(error)

struct QnRoutingManagementColors {
    QnRoutingManagementColors();

    QColor readOnly;
};
#define QnRoutingManagementColors_Fields (readOnly)

struct QnAuditLogColors {
    QnAuditLogColors();

    QColor httpLink;

    QColor loginAction;
    QColor unsucessLoginAction;
    QColor updUsers;
    QColor watchingLive;
    QColor watchingArchive;
    QColor exportVideo;
    QColor updCamera;
    QColor systemActions;
    QColor updServer;
    QColor eventRules;
    QColor emailSettings;

    QColor chartColor;
};
#define QnAuditLogColors_Fields (httpLink)(loginAction)(unsucessLoginAction)(updUsers)(watchingLive)(watchingArchive)(exportVideo)(updCamera)(systemActions)(updServer)(eventRules)(emailSettings)(chartColor)

struct QnRecordingStatsColors {
    QnRecordingStatsColors();

    QColor chartMainColor;
    QColor chartForecastColor;
};
#define QnRecordingStatsColors_Fields (chartMainColor)(chartForecastColor)

struct QnUserManagementColors {
    QnUserManagementColors();

    QColor disabledSelectedText;
    QColor disabledButtonsText;
    QColor selectionBackground;
};
#define QnUserManagementColors_Fields (disabledSelectedText)(disabledButtonsText)(selectionBackground)

struct QnServerUpdatesColors {
    QnServerUpdatesColors();

    QColor latest;
    QColor target;
    QColor error;
};
#define QnServerUpdatesColors_Fields (latest)(target)(error)

struct QnBackupScheduleColors {
    QnBackupScheduleColors();

    QColor weekEnd;
};
#define QnBackupScheduleColors_Fields (weekEnd)

struct QnFailoverPriorityColors {
    QnFailoverPriorityColors();

    QColor never;
    QColor low;
    QColor medium;
    QColor high;
};
#define QnFailoverPriorityColors_Fields (never)(low)(medium)(high)

struct QnGraphicsMessageBoxColors {
    QnGraphicsMessageBoxColors();

    QColor text;
    QColor frame;
    QColor window;
};
#define QnGraphicsMessageBoxColors_Fields (text)(frame)(window)

#define QN_CLIENT_COLOR_TYPES                                                   \
    (QnTimeSliderColors)(QnTimeScrollBarColors)(QnBackgroundColors)(QnCalendarColors) \
    (QnStatisticsColors)(QnScheduleGridColors)(QnGridColors)(QnPtzManageModelColors) \
    (QnHistogramColors)(QnResourceWidgetFrameColors)(QnLicensesListModelColors) \
    (QnRoutingManagementColors)(QnAuditLogColors)(QnRecordingStatsColors)(QnVideowallManageWidgetColors) \
    (QnUserManagementColors) \
    (QnServerUpdatesColors)(QnIoModuleColors) \
    (QnBackupScheduleColors) \
    (QnFailoverPriorityColors) \
    (QnBookmarkColors) \
    (QnCompositeTextOverlayColors) \
    (QnGraphicsMessageBoxColors) \


QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    QN_CLIENT_COLOR_TYPES,
    (metatype)(json)(eq)


);

#endif // QN_CLIENT_COLOR_TYPES_H

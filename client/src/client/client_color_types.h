#ifndef QN_CLIENT_COLOR_TYPES_H
#define QN_CLIENT_COLOR_TYPES_H

#include <QtCore/QMetaType>
#include <QtGui/QColor>

#include <utils/common/model_functions_fwd.h>

struct QnTimeSliderColors {
public:
    QnTimeSliderColors();

    QColor tickmark;
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

    QColor separator;

    QColor dateOverlay;
    QColor dateOverlayAlternate;

    QColor pastLastMinute;
    QColor futureLastMinute;
};
#define QnTimeSliderColors_Fields (tickmark)(positionMarker)(indicator)(selection)(selectionMarker)\
    (pastBackground)(futureBackground)(pastRecording)(futureRecording)(pastMotion)(futureMotion)\
    (separator)(dateOverlay)(dateOverlayAlternate)(pastLastMinute)(futureLastMinute)\
    (pastBookmark)(futureBookmark)

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
    QColor primaryMotion;
    QColor secondaryMotion;
    QColor separator;

    /* These are defined in calendar_item_delegate.cpp. */
    QColor primary(int fillType) const;
    QColor secondary(int fillType) const;
};
#define QnCalendarColors_Fields (selection)(primaryRecording)(secondaryRecording)(primaryMotion)(secondaryMotion)(separator)


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

struct QnServerUpdatesColors {
    QnServerUpdatesColors();

    QColor latest;
    QColor target;
    QColor error;
};
#define QnServerUpdatesColors_Fields (latest)(target)(error)

#define QN_CLIENT_COLOR_TYPES                                                   \
    (QnTimeSliderColors)(QnTimeScrollBarColors)(QnBackgroundColors)(QnCalendarColors) \
    (QnStatisticsColors)(QnScheduleGridColors)(QnGridColors)(QnPtzManageModelColors) \
    (QnHistogramColors)(QnResourceWidgetFrameColors)(QnLicensesListModelColors) \
    (QnRoutingManagementColors)(QnVideowallManageWidgetColors) \
    (QnServerUpdatesColors)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    QN_CLIENT_COLOR_TYPES,
    (metatype)(json)
);

#endif // QN_CLIENT_COLOR_TYPES_H

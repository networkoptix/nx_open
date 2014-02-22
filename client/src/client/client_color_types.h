#ifndef QN_CLIENT_COLOR_TYPES
#define QN_CLIENT_COLOR_TYPES

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

    QColor separator;

    QColor dateOverlay;
    QColor dateOverlayAlternate;

    QColor pastLastMinute;
    QColor futureLastMinute;
};

struct QnTimeScrollBarColors {
    QnTimeScrollBarColors();

    QColor indicator;
    QColor border;
    QColor handle;
};

struct QnBackgroundColors {
    QnBackgroundColors();

    QColor normal;
    QColor panic;
};

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

struct QnStatisticsColors {
    QnStatisticsColors();

    QColor grid;
    QColor frame;
    QColor cpu;
    QColor ram;
    QColor networkLimit;
    QVector<QColor> hdds;
    QVector<QColor> network;

    QColor hddByKey(const QString &key) const;
    QColor networkByKey(const QString &key) const;
};

struct QnScheduleGridColors {
public:
    QnScheduleGridColors();

    QColor normalLabel;
    QColor weekendLabel;
    QColor selectedLabel;
    QColor disabledLabel;
};

struct QnGridColors {
    QnGridColors();

    QColor grid;
    QColor allowed;
    QColor disallowed;
};

struct QnPtzManageModelColors {
    QnPtzManageModelColors();

    QColor title;
    QColor invalid;
    QColor warning;
};

QN_DECLARE_FUNCTIONS_FOR_TYPES(
    (QnTimeSliderColors)(QnTimeScrollBarColors)(QnBackgroundColors)(QnCalendarColors)(QnStatisticsColors)(QnScheduleGridColors)(QnGridColors)(QnPtzManageModelColors), 
    (metatype)(json)
);

#endif // QN_CLIENT_COLOR_TYPES

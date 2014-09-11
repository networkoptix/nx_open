#include "client_color_types.h"

#include <ui/style/globals.h>

#include <utils/math/color_transformations.h>
#include <utils/serialization/json_functions.h>
#include <utils/fusion/fusion_adaptor.h>
#include <utils/math/math.h>

QnTimeSliderColors::QnTimeSliderColors() {
    tickmark = QColor(255, 255, 255, 255);
    positionMarker = QColor(255, 255, 255, 196);
    indicator = QColor(128, 160, 192, 128);

    selection = QColor(0, 150, 255, 110);
    selectionMarker = selection.lighter();

    pastBackground = QColor(255, 255, 255, 24);
    futureBackground = QColor(0, 0, 0, 64);

    pastRecording = QColor(64, 255, 64, 128);
    futureRecording = QColor(64, 255, 64, 64);

    pastMotion = QColor(255, 0, 0, 128);
    futureMotion = QColor(255, 0, 0, 64);

    pastBookmark = QColor(0, 0, 255, 128);
    futureBookmark = QColor(0, 0, 255, 64);

    separator = QColor(255, 255, 255, 64);

    dateOverlay = QColor(255, 255, 255, 48);
    dateOverlayAlternate = withAlpha(selection, 48);

    pastLastMinute = QColor(24, 24, 24, 255);
    futureLastMinute = QColor(0, 0, 0, 255);
}

QnTimeScrollBarColors::QnTimeScrollBarColors() {
    indicator = QColor(255, 255, 255, 255);
    border = QColor(255, 255, 255, 64);
    handle = QColor(255, 255, 255, 48);
}

QnBackgroundColors::QnBackgroundColors() {
    normal = QColor(26, 26, 240, 40);
    panic = QColor(255, 0, 0, 255);
}

QnCalendarColors::QnCalendarColors() {
    selection = QColor(0, 150, 255, 192);
    primaryRecording = QColor(32, 128, 32, 255);
    secondaryRecording = QColor(32, 255, 32, 255);
    primaryMotion = QColor(128, 0, 0, 255);
    secondaryMotion = QColor(255, 0, 0, 255);
    separator = QColor(0, 0, 0, 255);
}

QnStatisticsColors::QnStatisticsColors() {
    grid = QColor(66, 140, 237, 100);
    frame = QColor(66, 140, 237);
    cpu = QColor(66, 140, 237);
    ram = QColor(219, 59, 169);
    
    hdds 
        << QColor(237, 237, 237)   // C: sda
        << QColor(237, 200, 66)    // D: sdb
        << QColor(103, 237, 66)    // E: sdc
        << QColor(255, 131, 48)    // F: sdd
        << QColor(178, 0, 255)     // etc
        << QColor(0, 255, 255)
        << QColor(38, 127, 0)
        << QColor(255, 127, 127)
        << QColor(201, 0, 0);

    network 
        << QColor(255, 52, 52)
        << QColor(240, 255, 52)
        << QColor(228, 52, 255)
        << QColor(255, 52, 132);
}

QnScheduleGridColors::QnScheduleGridColors() {
    normalLabel =   QColor(255, 255, 255, 255);
    weekendLabel =  QColor(255, 128, 128, 255);
    selectedLabel = QColor(64,  128, 192, 255);
    disabledLabel = QColor(183, 183, 183, 255);
}

QnGridColors::QnGridColors() {
    grid = QColor(0, 240, 240, 128);
    allowed = QColor(0, 255, 0, 64);
    disallowed = QColor(255, 0, 0, 64);
}

QnPtzManageModelColors::QnPtzManageModelColors() {
    title = QColor(64, 64, 64);
    invalid = QColor(64, 16, 16);
    warning = QColor(64, 64, 16);
}

QnHistogramColors::QnHistogramColors() {
    background = QColor(0, 0, 0);
    border = QColor(96, 96, 96);
    histogram = QColor(192, 192, 192);
    selection = QColor(0, 150, 255, 110);
    grid = QColor(0, 128, 0, 128);
    text = QColor(255, 255, 255);
}

QnResourceWidgetFrameColors::QnResourceWidgetFrameColors() {
    normal = QColor(128, 128, 128, 196);
    active = normal.lighter();
    selected = QColor(64, 130, 180, 128);
}

QnLicensesListModelColors::QnLicensesListModelColors() {
    normal = QColor(255, 255, 255, 255);
    warning = QColor(Qt::yellow);
    expired = QColor(Qt::red);
}

QnRoutingManagementColors::QnRoutingManagementColors() {
    readOnly = Qt::lightGray;
    warningColor = Qt::red;
}

QnVideowallManageWidgetColors::QnVideowallManageWidgetColors() {
    desktop = Qt::darkGray;
    freeSpace = Qt::lightGray;
    item = QColor(64, 130, 180);
    text = Qt::white;
    error = Qt::red;
}


QnServerUpdatesColors::QnServerUpdatesColors() {
    latest = Qt::green;
    target = Qt::yellow;
    error = Qt::red;
}


QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    QN_CLIENT_COLOR_TYPES,
    (json),
    _Fields,
    (optional, true)
)

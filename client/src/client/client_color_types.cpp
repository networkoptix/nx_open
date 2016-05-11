#include "client_color_types.h"

#include <ui/style/globals.h>

#include <utils/math/color_transformations.h>
#include <utils/serialization/json_functions.h>
#include <utils/fusion/fusion_adaptor.h>
#include <utils/math/math.h>
#include <utils/common/model_functions.h>

QnTimeSliderColors::QnTimeSliderColors() {
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

    pastBookmark = QColor("#b21083dc");
    futureBookmark = QColor("#b21083dc");

    pastBookmarkBound = QColor("#1c8fe7");
    futureBookmarkBound = QColor("#1c8fe7");

    separator = QColor(255, 255, 255, 64);

    dateBarBackgrounds.push_back(QColor(255, 255, 255, 48));
    dateBarText = QColor(255, 255, 255, 255);

    pastLastMinuteBackground = QColor(24, 24, 24, 127);
    futureLastMinuteBackground = QColor(0, 0, 0, 127);
    pastLastMinuteStripe = QColor(24, 24, 24, 38);
    futureLastMinuteStripe = QColor(0, 0, 0, 38);

    tickmarkLines.push_back(QColor(255, 255, 255, 255));
    tickmarkText.push_back(QColor(255, 255, 255, 255));
}

QnTimeScrollBarColors::QnTimeScrollBarColors() {
    indicator = QColor(255, 255, 255, 255);
    border = QColor(255, 255, 255, 64);
    handle = QColor(255, 255, 255, 48);
}

QnBackgroundColors::QnBackgroundColors() {
    normal = QColor(0, 0, 255, 51);
    panic = QColor(255, 0, 0, 255);
}

QnCalendarColors::QnCalendarColors()
    : selection(0, 150, 255, 192)
    , primaryRecording(32, 128, 32, 255)
    , secondaryRecording(32, 255, 32, 255)
    , primaryBookmark(55, 117, 196, 255)
    , secondaryBookmark(105, 164, 240, 255)
    , primaryMotion(128, 0, 0, 255)
    , secondaryMotion(255, 0, 0, 255)
    , separator(0, 0, 0, 255)
{
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

QnIoModuleColors::QnIoModuleColors() {
    idLabel = QColor(0x57, 0x57, 0x57);
    buttonBackground = QColor("#1c1c1c");
}

QnScheduleGridColors::QnScheduleGridColors() {
    normalLabel =   QColor(255, 255, 255, 255);
    weekendLabel =  QColor(255, 128, 128, 255);
    selectedLabel = QColor(64,  128, 192, 255);
    disabledLabel = QColor(183, 183, 183, 255);

    recordNever =   QColor(64,  64,  64);
    recordAlways =  QColor(0,   100, 0);
    recordMotion =  QColor(100, 0,   0);
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

QnTwoWayAudioWidgetColors::QnTwoWayAudioWidgetColors() :
    background("#212a2f"),
    visualizer("#cf2727")
{
}

QnResourceWidgetFrameColors::QnResourceWidgetFrameColors() {
    normal = QColor(128, 128, 128, 196);
    active = normal.lighter();
    selected = QColor(64, 130, 180, 128);
}

QnBookmarkColors::QnBookmarkColors()
    : tooltipBackground("#295E87")
    , background("#b22e6996")
    , text(Qt::white)
    , buttonsSeparator("#3E6E93")
    , bookmarksSeparatorTop("#154163")
    , bookmarksSeparatorBottom("#3e6e93")
    , tagBgNormal("#3E6E93")
    , tagBgHovered("#547E9F")
    , tagTextNormal("#D8E2E9")
    , tagTextHovered("#ffffff")
    , moreItemsText("#94AEC3")
{
}

QnCompositeTextOverlayColors::QnCompositeTextOverlayColors()
    : bookmarkColors()
    , textOverlayItemColor(0, 0, 0, 128)
{
}

QnLicensesListModelColors::QnLicensesListModelColors() {
    normal = QColor(255, 255, 255, 255);
    warning = QColor(Qt::yellow);
    expired = QColor(Qt::red);
}

QnRoutingManagementColors::QnRoutingManagementColors() {
    readOnly = Qt::lightGray;
}

QnAuditLogColors::QnAuditLogColors() {
    httpLink = QColor(0x2f, 0xa2, 0xdb);

    unsucessLoginAction = QColor(0xcf, 0x27, 0x27);

    loginAction = QColor(0x65, 0x99, 0x1c);
    updUsers = QColor(0x99, 0x49, 0x98); // QColor(0x02, 0x9e, 0xb0);
    watchingLive = QColor(0x02, 0x9e, 0xb0); //QColor(0xb1, 0x30, 0x16);
    watchingArchive = watchingLive;
    exportVideo = QColor(0x5d, 0x82, 0xbf);
    updCamera = QColor(0xf3, 0x82, 0x82);
    systemActions = QColor(0xb1, 0x30, 0x16); //QColor(0xdc, 0xb4, 0x28);
    updServer = QColor(0xdc, 0xb4, 0x28); //QColor(0x99, 0x49, 0x98);
    eventRules = QColor(0x9c, 0x6b, 0x50); //QColor(0x5d, 0x82, 0xbf);
    emailSettings = QColor(0xcb, 0xcb, 0xcb); //QColor(0x9c, 0x6b, 0x50);

    chartColor = QColor(0x25, 0x92, 0xc3);
}

QnRecordingStatsColors::QnRecordingStatsColors()
{
    chartMainColor = QColor(0x25, 0x92, 0xc3);
    chartForecastColor = QColor(0x0c, 0x51, 0x69);
}


QnUserManagementColors::QnUserManagementColors()
{
    disabledSelectedText = QColor(0xb5, 0xd7, 0xee);
    disabledButtonsText = QColor(0xff, 0xff, 0xff);
    selectionBackground = QColor(0x22, 0x53, 0x77);
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

QnBackupScheduleColors::QnBackupScheduleColors() {
    weekEnd = QColor(0xff8080);
}

QnFailoverPriorityColors::QnFailoverPriorityColors() {
    never   = QColor(0x808080);
    low     = QColor(0x9eb236);
    medium  = QColor(0xcc853d);
    high    = QColor(0xff5500);
}

QnGraphicsMessageBoxColors::QnGraphicsMessageBoxColors()
{
    text    = QColor(0xff, 0xff, 0xff, 0x99);
    frame   = QColor(0x00, 0x7e, 0xd4, 0xff);
    window  = QColor(0x00, 0x4b, 0x80, 0xcc);
}

QnResourceItemColors::QnResourceItemColors()
{
    mainText = QColor(0x91a7b2);
    mainTextSelected = QColor(0xe1e7eA);
    mainTextAccented = QColor(0x2fa2db);
    extraText = QColor(0x53707f);
    extraTextSelected = QColor(0xa5b7c0);
    extraTextAccented = QColor(0x117297);
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    QN_CLIENT_COLOR_TYPES,
    (json)(eq),
    _Fields,
    (optional, true)
);

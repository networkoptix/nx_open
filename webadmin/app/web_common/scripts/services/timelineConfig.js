'use strict';
/*exported TimelineConfig */

var TimelineConfig = {
    initialInterval: 1000 * 60 * 60 /* *24*365*/, // no records - show small interval
    stickToLiveMs: 5 * 1000, // Value to stick viewpoert to Live - 5 seconds
    maxMsPerPixel: 1000 * 60 * 60 * 24 * 365,   // one year per pixel - maximum view
    lastMinuteDuration: 1.5 * 60 * 1000, // 1.5 minutes
    minMsPerPixel: 10, // Minimum level for zooming:
    lastMinuteAnimationMs:30,
    minMarkWidth: 1, // Minimum width for visible mark
    smoothMoveLength: 3, //Minimum move of timeline which requires smooth moving
    hideTimeMarkerAfterClickedDistance: 5, // distance around click we do not show timemarker under pointer

    zoomSpeed: 0.025, // Zoom speed for dblclick
    zoomAccuracyMs: 5000, // 5 seconds - accuracy for zoomout
    slowZoomSpeed: 0.02, // Zoom speed for holding buttons
    scrollSliderMoveDuration: 3000, // seconds for scroll slider to catch the mouse
    maxVerticalScrollForZoom: 250, // value for adjusting zoom
    maxVerticalScrollForZoomWithTouch: 5000, // value for adjusting zoom
    animationDuration: 400, // 300, // 200-400 for smooth animation

    levelsSettings:{
        labels:{
            markSize:17/100,
            transparency:1,
            fontSize:14,
            labelPositionFix:5
        },
        middle:{
            markSize:11/100,
            transparency:0.8,
            fontSize:13,
            labelPositionFix:0
        },
        small:{
            markSize:6/100,
            transparency:0.6,
            fontSize:11,
            labelPositionFix:-5
        },
        marks:{
            markSize:6/110,
            transparency:0.3,
            fontSize:0,
            labelPositionFix: -10
        },
        events:{
            markSize:0,
            transparency:0,
            fontSize:0,
            labelPositionFix:-15
        }
    },

    timelineBgColor: [28,35,39,0.6], //Color for ruler marks and labels  //$dark6
    font:'Roboto',//'sans-serif',

    labelPadding: 10,
    lineWidth: 1,

    chunkHeight:24/100, // %    //Height for event line
    minChunkWidth: 1,
    minPixelsPerLevel: 1,
    chunksBgColor:[235,239,241],  //$light3
    chunksBgBorderColor: [225,231,234],  //$light4
    exactChunkColor: [76,188,40], //Archive  //$green_l2
    loadingChunkColor: [76,188,40, 0.7], //#00FFFF this is not in palette
    blindChunkColor:  [255,0,255,0.3],  //#FF00FF this is not in palette
    highlighChunkColor: [255,255,0,1],  //#FFFF00 this is not in palette
    lastMinuteColor: [58,145,30,0.3],   //$green_main

    scrollBarSpeed: 0.1, // By default - scroll by 10%
    scrollBarHeight: 14/100, // %
    scrollBarTopPadding: 2/100,
    minScrollBarWidth: 40, // px
    scrollBarBgColor: [255,255,255],  //$light1
    scrollBarColor: [145,167,178],  //$light12
    scrollBarHighlightColor: [165,183,192],  //$light10
    scrollBarHighlightColorActive: [155,175,185],  //$light11


    //Scroll bar marks
    scrollBarMarksColor : [105,135,150],  //$light16
    scrollBarMarksHeightOffset : 4,
    scrollBarMarksSpacing : 4,

    timeMarkerColor: [43,56,63, 0.9], // Timemarker color //$dark9
    timeMarkerTextColor: [255,255,255],  //$light1
    pointerMarkerColor: [205,215,220, 0.9], // Mouse pointer marker color //$light6
    pointerMarkerTextColor: [43,56,63],  //$dark9
    timeMarkerLineWidth: 1,
    timeMarkerPointerLineWidth: 2,
    markerDateFont:{
        size:13,
        weight:400,
        face:'Roboto'
    },
    markerTimeFont:{
        size:16,
        weight:700,
        face:'Roboto'
    },
    markerWidth: 140,
    markerHeight: 48/100,
    markerPullDown: 3,
    markerTriangleHeight: 3/100,
    markerLineHeightOffset: 4,
    markerTimeOffset: 6,

    dateFormat: 'd mmmm yyyy', // Timemarker format for date
    timeFormat: 'HH:MM:ss', // Timemarker format for time
    timeFormatAMPM: 'h:MM:ss tt',
    hourFormat: 'HH:MM',    // Time format without seconds
    hourFormatAMPM: 'h:MM tt',

    scrollButtonsColor: [105,135,150,0.3],  //$light16
    scrollButtonsHoverColor: [105,135,150,0.5],  //$light16
    scrollButtonsActiveColor: [105,135,150,0.4],  //$light16
    scrollButtonsWidth: 24,
    borderAreaWidth: 64,
    scrollButtonsHeight: 48/100,
    scrollButtonsArrowLineWidth:2,
    scrollButtonsArrowColor:[255,255,255,0.8],  //$light1
    scrollButtonsArrowActiveColor:[255,255,255,1],  //$light1
    scrollButtonsArrow:{
        size:20,
        width:10
    },
    scrollButtonMarginBottom: 4,
    scrollSpeed: 1, // Relative to screen - one click on scrollbar
    scrollButtonSpeed: 0.25, // One click on scrollbutton
    slowScrollSpeed: 0.25,
    intertiaFriction: 0.92, // Friction for scrolling and drag and drop


    scrollBoundariesPrecision: 0.000001, // Where we should disable right and left scroll buttons
    end: 0,

    // rulerPreset
    topLabelAlign: 'center', // center, left, above
    topLabelHeight: 20/100, //% // Size for top label text
    topLabelFont:{
        size:13,
        weight:400,
        face:'Roboto',
        color: [43,56,63] // Color for text for top labels //$dark9
    },
    topLabelMarkerColor: [105,135,150], // Color for mark for top label //$light16
    topLabelMarkerAttach:'top', // top, bottom
    topLabelMarkerHeight: 20/100,
    topLabelBgColor: [255,255,255], //$light1
    topLabelBgOddColor: [240,243,244], //$additional_light2
    topLabelPositionFix:-2, //Vertical fix for position
    topLabelFixed: true,
    topLabelBottomBorderColor: [73,98,111,0.3],  //$dark15

    labelAlign:'above',// center, left, above
    labelHeight:40/100, // %
    labelFont:{
        size:15,
        weight:400,
        face:'Roboto',
        color: [105,135,150] // Color for text for top labels //$light16
    },
    labelMarkerAttach:'top', // top, bottom
    labelMarkerColor:[105,135,150],//false  //$light16
    labelBgColor: [28,35,39],  //$dark6
    labelPositionFix:5, //Vertical fix for position
    labelMarkerHeight:10/100,
    labelFixed: false,


    marksAttach:'top', // top, bottom
    marksHeight:5/100,
    marksColor:[83,112,127,0.6], //$dark_core
    oldStyle:true
};



/**
    Here we detect if locale uses AM/PM
*/
(function(){
    var dateFormat = (new Date).toLocaleString();
    var regex = /(am|pm)$/gi;
    if(regex.test(dateFormat)){
        TimelineConfig.hourFormat = TimelineConfig.hourFormatAMPM;
        TimelineConfig.timeFormat = TimelineConfig.timeFormatAMPM;
    };
})();

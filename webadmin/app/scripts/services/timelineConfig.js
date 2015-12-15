'use strict';


function TimelineConfig(){

    var rulerPreset = {
        topLabelAlign: 'center', // center, left, above
        topLabelHeight: 30/110, //% // Size for top label text
        topLabelFont:{
            size:17,
            weight:400,
            face:'Roboto',
            color: [255,255,255] // Color for text for top labels
        },
        topLabelMarkerColor: [83,112,127], // Color for mark for top label
        topLabelMarkerAttach:'top', // top, bottom
        topLabelMarkerHeight: 55/110,
        topLabelBgColor: false,
        topLabelBgOddColor: false,
        topLabelPositionFix:0, //Vertical fix for position
        topLabelFixed: true,
        oldStyle:false,

        labelAlign:'above',// center, left, above
        labelHeight:30/110, // %
        labelFont:{
            size:15,
            weight:400,
            face:'Roboto',
            color: [105,135,150] // Color for text for top labels
        },
        labelMarkerAttach:'bottom', // top, bottom
        labelMarkerColor:[83,112,127,0.6],//false
        labelBgColor: [28,35,39],
        labelPositionFix:-5, //Vertical fix for position
        labelMarkerHeight:22/110,
        labelFixed: true,


        marksAttach:'bottom', // top, bottom
        marksHeight:10/110,
        marksColor:[83,112,127,0.4]
    };

    var timelineConfig = {
        initialInterval: 1000 * 60 * 60 /* *24*365*/, // no records - show small interval
        stickToLiveMs: 10 * 1000, // Value to stick viewpoert to Live - 10 seconds
        maxMsPerPixel: 1000 * 60 * 60 * 24 * 365,   // one year per pixel - maximum view
        lastMinuteDuration: 1.5 * 60 * 1000, // 1.5 minutes
        minMsPerPixel: 10, // Minimum level for zooming:
        lastMinuteAnimationMs:100,
        lastMinuteTextureSize:10,
        minMarkWidth: 1, // Minimum width for visible mark
        smoothMoveLength: 3, //Minimum move of timeline which requires smooth moving

        zoomSpeed: 0.025, // Zoom speed for dblclick
        zoomAccuracyMs: 5000, // 5 seconds - accuracy for zoomout
        slowZoomSpeed: 0.01, // Zoom speed for holding buttons
        maxVerticalScrollForZoom: 250, // value for adjusting zoom
        maxVerticalScrollForZoomWithTouch: 5000, // value for adjusting zoom
        animationDuration: 400, // 300, // 200-400 for smooth animation

        levelsSettings:{
            labels:{
                markSize:15/110,
                transparency:1,
                fontSize:14,
                labelPositionFix:5
            },
            middle:{
                markSize:10/110,
                transparency:0.75,
                fontSize:13,
                labelPositionFix:0
            },
            small:{
                markSize:5/110,
                transparency:0.5,
                fontSize:11,
                labelPositionFix:-5
            },
            marks:{
                markSize:5/110,
                transparency:0.25,
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

        timelineBgColor: [28,35,39,0.6], //Color for ruler marks and labels
        font:'Roboto',//'sans-serif',

        labelPadding: 10,
        lineWidth: 1,

        chunkHeight:30/110, // %    //Height for event line
        minChunkWidth: 0,
        minPixelsPerLevel: 1,
        chunksBgColor:[34,57,37],
        exactChunkColor: [58,145,30],
        loadingChunkColor: [0,255,255,0.3],
        blindChunkColor:  [255,0,0,0.3],
        highlighChunkColor: [255,255,0,1],
        lastMinuteColor: [58,145,30,0.3],

        scrollBarSpeed: 0.1, // By default - scroll by 10%
        scrollBarHeight: 20/110, // %
        minScrollBarWidth: 50, // px
        scrollBarBgColor: [28,35,39],
        scrollBarColor: [53,70,79],
        scrollBarHighlightColor: [53,70,79,0.8],

        timeMarkerColor: [215,223,227], // Timemarker color
        timeMarkerTextColor: [12,21,23],
        pointerMarkerColor: [12,21,23], // Mouse pointer marker color
        pointerMarkerTextColor: [255,255,255],
        timeMarkerLineWidth: 1,
        markerDateFont:{
            size:15,
            weight:400,
            face:'Roboto'
        },
        markerTimeFont:{
            size:17,
            weight:500,
            face:'Roboto'
        },
        markerWidth: 140,
        markerHeight: 50/110,
        markerTriangleHeight: 6/110,
        dateFormat: 'd mmmm yyyy', // Timemarker format for date
        timeFormat: 'HH:MM:ss', // Timemarker format for time


        scrollButtonsColor: [23,28,31,0.8],
        scrollButtonsActiveColor: [23,28,31,1],
        scrollButtonsWidth: 40,
        scrollButtonsHeight: 50/110,
        scrollButtonsArrowLineWidth:2,
        scrollButtonsArrowColor:[255,255,255,0.8],
        scrollButtonsArrowActiveColor:[255,255,255,1],
        scrollButtonsArrow:{
            size:20,
            width:10
        },
        scrollSpeed: 1, // Relative to screen - one click on scrollbar
        scrollButtonSpeed: 0.25, // One click on scrollbutton 
        slowScrollSpeed: 0.25,


        scrollBoundariesPrecision: 0.000001, // Where we should disable right and left scroll buttons

        end: 0
    };


    var presets = [{
        topLabelAlign: 'center', // center, left, above
        topLabelMarkerColor: [105,135,150], // Color for mark for top label
        topLabelBgColor: false,
        topLabelBgOddColor: false,

        labelAlign:'above',// center, left, above
        labelMarkerColor:[53,70,79,0],//false
        labelBgColor: [28,35,39],
        oldStyle:false,

        marksColor:[83,112,127,0]
    },{
        topLabelAlign: 'center', // center, left, above
        topLabelMarkerColor: [105,135,150], // Color for mark for top label
        topLabelBgColor: false,
        topLabelBgOddColor: false,

        labelAlign:'above',// center, left, above
        labelMarkerColor:[83,112,127,0.6],//false
        labelBgColor: [28,35,39],
        oldStyle:false,

        marksColor:[83,112,127,0]
    }, {
        topLabelAlign: 'left', // center, left, above
        topLabelMarkerColor: [105,135,150], // Color for mark for top label
        topLabelBgColor: false,
        topLabelBgOddColor: false,

        labelAlign:'left',// center, left, above
        labelMarkerColor:[83,112,127,0.6],//false
        labelBgColor: [28,35,39],
        oldStyle:false,

        marksColor:[83,112,127,0.4]
    },{
        topLabelAlign: 'center', // center, left, above
        topLabelHeight: 20/110, //% // Size for top label text
        topLabelFont:{
            size:12,
            weight:400,
            face:'Roboto',
            color: [255,255,255] // Color for text for top labels
        },
        topLabelMarkerColor: [83,112,127], // Color for mark for top label
        topLabelMarkerHeight: 20/110,
        topLabelMarkerAttach:'top', // top, bottom
        topLabelBgColor: [33,42,47],
        topLabelBgOddColor: [43,56,63],
        topLabelPositionFix:-2, //Vertical fix for position
        topLabelFixed: true,


        labelAlign:'above',// center, left, above
        labelHeight:40/110, // %
        labelFont:{
            size:15,
            weight:400,
            face:'Roboto',
            color: [145,167,178] // Color for text for top labels
        },
        labelMarkerAttach:'top', // top, bottom
        labelMarkerColor:[105,135,150],//false
        labelBgColor: [28,35,39],
        labelPositionFix:5, //Vertical fix for position
        labelMarkerHeight:20/110,
        labelFixed: false,

        marksAttach:'top', // top, bottom
        marksHeight:10/110,
        marksColor:[83,112,127,0.4],
        oldStyle:true
    }];

    return $.extend(timelineConfig, rulerPreset, presets[3]);

}
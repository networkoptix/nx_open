'use strict';

angular.module('webadminApp')
    .directive('timeline', ['$interval','$timeout','animateScope', function ($interval,$timeout,animateScope, $log) {
        return {
            restrict: 'E',
            scope: {
                recordsProvider: '=',
                positionProvider: '=',
                playHandler: '=',
                liveOnly: '=',
                canPlayLive: '=',
                ngClick: '&',
                positionHandler: '='
            },
            templateUrl: 'views/components/timeline.html',
            link: function (scope, element/*, attrs*/) {

                var debugEventsMode = Config.debug.chunksOnTimeline && Config.allowDebugMode;
                var debugTime = Config.debug.chunksOnTimeline && Config.allowDebugMode;


                var timelineConfig = {
                    initialInterval: 1000*60*60 /* *24*365*/, // no records - show small interval
                    stickToLiveMs: 1000, // Value to stick viewpoert to Live - 1 second
                    maxMsPerPixel: 1000*60*60*24*365,   // one year per pixel - maximum view
                    lastMinuteDuration: 1.5 * 60 * 1000, // 1.5 minutes
                    minMsPerPixel: 10, // Minimum level for zooming:
                    lastMinuteAnimationMs:100,
                    lastMinuteTextureSize:10,
                    minMarkWidth: 1, // Minimum width for visible mark

                    zoomSpeed: 0.025, // Zoom speed for dblclick
                    zoomAccuracy: 0.01,
                    slowZoomSpeed: 0.01, // Zoom speed for holding buttons
                    maxVerticalScrollForZoom: 250, // value for adjusting zoom
                    maxVerticalScrollForZoomWithTouch: 5000, // value for adjusting zoom
                    animationDuration: 300, // 300, // 200-400 for smooth animation

                    levelsSettings:{
                        labels:{
                            markSize:15/110,
                            transparency:1,
                            fontSize:14,
                            labelPositionFix:5//20
                        },
                        middle:{
                            markSize:10/110,
                            transparency:0.75,
                            fontSize:13,
                            labelPositionFix:0//18
                        },
                        small:{
                            markSize:5/110,
                            transparency:0.5,
                            fontSize:11,
                            labelPositionFix:-5//16
                        },
                        marks:{
                            markSize:5/110,
                            transparency:0.25,
                            fontSize:0,
                            labelPositionFix:-10
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
                    minChunkWidth: 1,
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
                    markerDateFont:{
                        size:15,
                        weight:400,
                        face:"Roboto"
                    },
                    markerTimeFont:{
                        size:17,
                        weight:500,
                        face:"Roboto"
                    },
                    markerWidth: 140,
                    markerHeight: 50/110,
                    markerTriangleHeight: 6/110,
                    dateFormat: 'd mmmm yyyy', // Timemarker format for date
                    timeFormat: 'HH:MM:ss', // Timemarker format for time


                    leftRightButtonsColor: [23,28,31,0.8],
                    leftRightButtonsActiveColor: [23,28,31,1],
                    leftRightButtonsWidth: 40,
                    leftRightButtonsHeight: 50/110,
                    leftRightButtonsArrowLineWidth:2,
                    leftRightButtonsArrowColor:[255,255,255,0.8],
                    leftRightButtonsArrowActiveColor:[255,255,255,1],
                    leftRightButtonsArrow:{
                        size:20,
                        width:10
                    },
                    scrollSpeed:0.25, // Relative to screen
                    slowScrollSpeed: 0.25,


                    scrollBoundariesPrecision: 0.000001, // Where we should disable right and left scroll buttons

                    end: 0
                };

                scope.timelineConfig = timelineConfig;

                var rulerPreset = {
                    topLabelAlign: "center", // center, left, above
                    topLabelHeight: 30/110, //% // Size for top label text
                    topLabelFont:{
                        size:17,
                        weight:400,
                        face:"Roboto",
                        color: [255,255,255] // Color for text for top labels
                    },
                    topLabelMarkerColor: [83,112,127], // Color for mark for top label
                    topLabelMarkerAttach:"top", // top, bottom
                    topLabelMarkerHeight: 55/110,
                    topLabelBgColor: false,
                    topLabelBgOddColor: false,
                    topLabelPositionFix:0, //Vertical fix for position
                    topLabelFixed: true,
                    oldStyle:false,

                    labelAlign:"above",// center, left, above
                    labelHeight:30/110, // %
                    labelFont:{
                        size:15,
                        weight:400,
                        face:"Roboto",
                        color: [105,135,150] // Color for text for top labels
                    },
                    labelMarkerAttach:"bottom", // top, bottom
                    labelMarkerColor:[83,112,127,0.6],//false
                    labelBgColor: [28,35,39],
                    labelPositionFix:-5, //Vertical fix for position
                    labelMarkerHeight:22/110,
                    labelFixed: true,


                    marksAttach:"bottom", // top, bottom
                    marksHeight:10/110,
                    marksColor:[83,112,127,0.4]
                };


                scope.presets = [{
                    topLabelAlign: "center", // center, left, above
                    topLabelMarkerColor: [105,135,150], // Color for mark for top label
                    topLabelBgColor: false,
                    topLabelBgOddColor: false,

                    labelAlign:"above",// center, left, above
                    labelMarkerColor:[53,70,79,0],//false
                    labelBgColor: [28,35,39],
                    oldStyle:false,

                    marksColor:[83,112,127,0]
                },{
                    topLabelAlign: "center", // center, left, above
                    topLabelMarkerColor: [105,135,150], // Color for mark for top label
                    topLabelBgColor: false,
                    topLabelBgOddColor: false,

                    labelAlign:"above",// center, left, above
                    labelMarkerColor:[83,112,127,0.6],//false
                    labelBgColor: [28,35,39],
                    oldStyle:false,

                    marksColor:[83,112,127,0]
                }, {
                    topLabelAlign: "left", // center, left, above
                    topLabelMarkerColor: [105,135,150], // Color for mark for top label
                    topLabelBgColor: false,
                    topLabelBgOddColor: false,

                    labelAlign:"left",// center, left, above
                    labelMarkerColor:[83,112,127,0.6],//false
                    labelBgColor: [28,35,39],
                    oldStyle:false,

                    marksColor:[83,112,127,0.4]
                },{
                    topLabelAlign: "center", // center, left, above
                    topLabelHeight: 20/110, //% // Size for top label text
                    topLabelFont:{
                        size:12,
                        weight:400,
                        face:"Roboto",
                        color: [255,255,255] // Color for text for top labels
                    },
                    topLabelMarkerColor: [83,112,127], // Color for mark for top label
                    topLabelMarkerHeight: 20/110,
                    topLabelMarkerAttach:"top", // top, bottom
                    topLabelBgColor: [33,42,47],
                    topLabelBgOddColor: [43,56,63],
                    topLabelPositionFix:-2, //Vertical fix for position
                    topLabelFixed: true,


                    labelAlign:"above",// center, left, above
                    labelHeight:40/110, // %
                    labelFont:{
                        size:15,
                        weight:400,
                        face:"Roboto",
                        color: [145,167,178] // Color for text for top labels
                    },
                    labelMarkerAttach:"top", // top, bottom
                    labelMarkerColor:[105,135,150],//false
                    labelBgColor: [28,35,39],
                    labelPositionFix:5, //Vertical fix for position
                    labelMarkerHeight:20/110,
                    labelFixed: false,

                    marksAttach:"top", // top, bottom
                    marksHeight:10/110,
                    marksColor:[83,112,127,0.4],
                    oldStyle:true
                }];

                scope.selectPreset = function(preset){
                    scope.activePreset = preset;
                    scope.timelineConfig = $.extend(scope.timelineConfig, rulerPreset, preset);
                };
                scope.selectPreset(scope.presets[3]);

                /**
                 * This is main timeline module.
                 *
                 * Architecture:
                 *
                 * 1. There are three models, used for data exchange: positionProvider, recordsProvider, Boundaries&Zoom
                 * 2. There are four interface modules: Labels (Top and lower lines), Events, ScrollBar, TimeMarker
                 *
                 * positionProvider shows current playing position (interracts with video-player, can be changed with clicking on the timeline)
                 *      positionProvider also jumps between small chunks while playing.
                 *
                 * recordsProvider requests records from server with required detailization.
                 *      We ask recrodsProvider to create splices within boundaries
                 *
                 * ScaleManager represents current viewport of the timeline: which time interval is visible and current zoomlevel.
                 *      Also provides methods to convert screen position to time and time to position.
                 *      Also manages total boundaries of the timeline, including live view
                 *
                 *
                 *
                 * Labels - are two lines with dates above events line. Not interactive, just gets boundaries and draws labels
                 *
                 * Events shows chunks on the timeline, using splice from recordsProvider
                 *
                 * ScrollBar - manages boundaries. If we scroll - we change boundaries. Also scroll moves automatically while playing.
                 *
                 * TimeMarker just renders timemarker with playing position above timeline. Uses data from positionProvider
                 *
                 *
                 *
                 * animateScope - special code which calls drawing and supports smooth animations
                 */

                // !!! Read basic parameters, DOM elements and global objects for module
                var viewport = element.find('.viewport');
                var canvas = element.find('canvas').get(0);
                canvas.addEventListener('contextmenu', function(e) {
                    e.preventDefault();
                });

                scope.scaleManager = new ScaleManager( timelineConfig.minMsPerPixel,
                    timelineConfig.maxMsPerPixel,
                    timelineConfig.initialInterval,
                    100,
                    timelineConfig.stickToLiveMs,
                    timelineConfig.zoomAccuracy,
                    timelineConfig.lastMinuteDuration); //Init boundariesProvider

                // !!! Initialization functions
                function updateTimelineHeight(){
                    scope.viewportHeight = element.find(".viewport").height();
                    canvas.height = scope.viewportHeight;
                }
                function updateTimelineWidth(){
                    scope.viewportWidth = element.find(".viewport").width();
                    canvas.width  = scope.viewportWidth;
                    scope.scaleManager.setViewportWidth(scope.viewportWidth);
                    $timeout(checkZoomButtons);
                }
                function initTimeline(){
                    var now = (new Date()).getTime();
                    scope.scaleManager.setStart(scope.recordsProvider && scope.recordsProvider.chunksTree ? scope.recordsProvider.chunksTree.start : (now - timelineConfig.initialInterval));
                    scope.scaleManager.setEnd(now);

                    scope.zoomTo(1); // Animate full zoom out
                }

                // !!! Drawing and redrawing functions
                function drawAll(){
                    if(scope.positionProvider) {
                        scope.scaleManager.tryToSetLiveDate(scope.positionProvider.playedPosition,scope.positionProvider.liveMode,(new Date()).getTime());
                    }
                    if(scope.recordsProvider) {
                        scope.recordsProvider.updateLastMinute(timelineConfig.lastMinuteDuration, scope.scaleManager.levels.events.index);
                    }


                    processZooming();
                    processScrolling();

                    var context = clearTimeline();
                    drawTopLabels(context);

                    drawLabels(context);

                    if(!debugEventsMode) {
                        drawOrCheckEvents(context);
                    }else{
                        debugEvents(context);
                    }

                    drawLastMinute(context);

                    drawOrCheckScrollBar(context);
                    drawTimeMarker(context);
                    drawPointerMarker(context);

                    drawOrCheckLeftRightButtons(context);

                    //
                }

                function blurColor(color,alpha){ // Bluring function [r,g,b] + alpha -> String

                    if(typeof(color)=="string"){ // do not try to change strings
                        return color;
                    }
                    if(typeof(alpha) == 'undefined'){
                        alpha = 1;
                    }
                    if(alpha > 1) { // Not so good. It's hack. Somewhere
                        console.error('bad value', alpha);
                    }

                    if(color.length > 3){ //Array already has alpha channel
                        alpha = alpha * color[3];
                    }

                    var colorString =  'rgba(' +
                        Math.round(color[0]) + ',' +
                        Math.round(color[1]) + ',' +
                        Math.round(color[2]) + ',' +
                        alpha + ')';

                    return colorString;
                }
                function formatFont(font){
                    return font.weight + " " + Math.round(font.size) + "px " + font.face;
                }

                function clearTimeline(){
                    var context = canvas.getContext('2d');
                    context.fillStyle = blurColor(timelineConfig.timelineBgColor,1);
                    context.clearRect(0, 0, scope.viewportWidth, scope.viewportHeight);
                    context.lineWidth = timelineConfig.lineWidth;
                    return context;
                }

                // !!! Labels logic
                function drawTopLabels(context){
                    drawLabelsRow(context,
                        scope.scaleManager.levels.top.index,
                        scope.scaleManager.levels.top.index,
                        1,
                        "topFormat",
                        timelineConfig.topLabelFixed,
                        0,
                        timelineConfig.topLabelHeight,
                        timelineConfig.topLabelFont,
                        timelineConfig.topLabelAlign,
                        timelineConfig.topLabelBgColor,
                        timelineConfig.topLabelBgOddColor,
                        timelineConfig.topLabelMarkerColor,
                        timelineConfig.topLabelPositionFix,
                        timelineConfig.topLabelMarkerAttach,
                        timelineConfig.topLabelMarkerHeight);
                }

                scope.changingLevel = 1;
                function drawLabels(context){
                    if(timelineConfig.oldStyle) {
                        drawLabelsOldStyle(
                            context,
                            "format",
                            timelineConfig.labelFixed,
                            timelineConfig.topLabelHeight,
                            timelineConfig.labelHeight,
                            timelineConfig.labelFont,
                            timelineConfig.labelAlign,
                            timelineConfig.labelBgColor,
                            timelineConfig.labelBgColor,
                            timelineConfig.labelMarkerColor,
                            timelineConfig.labelMarkerAttach,
                            timelineConfig.levelsSettings);

                        return;
                    }

                    drawLabelsRow(
                        context,
                        currentLevels.labels.index,
                        targetLevels.labels.index,
                        scope.zooming,
                        "format",
                        timelineConfig.labelFixed,
                        timelineConfig.topLabelHeight,
                        timelineConfig.labelHeight,
                        timelineConfig.labelFont,
                        timelineConfig.labelAlign,
                        timelineConfig.labelBgColor,
                        timelineConfig.labelBgColor,
                        timelineConfig.labelMarkerColor,
                        timelineConfig.labelPositionFix,
                        timelineConfig.labelMarkerAttach,
                        timelineConfig.labelMarkerHeight);

                    drawMarks(context);
                }

                function drawMarks(context){
                    drawLabelsRow(
                        context,
                        currentLevels.marks.index,
                        targetLevels.marks.index,
                        scope.zooming,
                        false,// No format
                        "none",
                        timelineConfig.topLabelHeight,
                        timelineConfig.labelHeight,
                        null,//font
                        null,//align
                        null,//no bg label
                        null,//no bg label
                        timelineConfig.marksColor,
                        0,
                        timelineConfig.marksAttach,
                        timelineConfig.marksHeight );
                }


                var targetLevels = scope.scaleManager.levels;
                var currentLevels = {"events":{index:0},"labels":{index:0},"middle":{index:0},"small":{index:0},"marks":{index:0}};
                function drawLabelsOldStyle(context,
                    labelFormat, labelFixed, levelTop, levelHeight,
                    font, labelAlign, bgColor, bgOddColor, markColor, markAttach, levelsSettings){
                    // 1. we need four levels:
                    // Mark only
                    // small label
                    // middle label
                    // Large label

                    //2. Every level can be animated
                    //3. Animation includes changing of height and font-size and transparency for colors

                    function getPointAnimation(pointLevelIndex, levelName){ // Get transition value for animation for this level
                        var animation = scope.zooming;

                        var minLevel = getLevelMinValue(levelName);

                        if( pointLevelIndex <= minLevel ) {
                            return null; // Do not animate
                        }

                        if(targetLevels[levelName].index == currentLevels[levelName].index) { // Do not animate
                            return null;
                        }

                        if(targetLevels[levelName].index > currentLevels[levelName].index){ // Increasing
                            return 1-animation;
                        }

                        return animation;
                    }

                    function getLevelMinValue(levelName){ // Get lower levelinex for this level
                        return Math.min(currentLevels[levelName].index,targetLevels[levelName].index);
                    }

                    function getLevelMaxValue(levelName){
                        return Math.max(currentLevels[levelName].index,targetLevels[levelName].index);
                    }

                    function getBestLevelName(pointLevelIndex){ // Find best level for this levelindex
                        if(pointLevelIndex <= getLevelMaxValue("labels"))
                            return "labels";

                        if(pointLevelIndex <= getLevelMaxValue("middle"))
                            return "middle";

                        if(pointLevelIndex <= getLevelMaxValue("small"))
                            return "small";

                        if(pointLevelIndex <= getLevelMaxValue("marks"))
                            return "marks";

                        return "events";
                    }

                    function getPrevLevelName(pointLevelIndex){
                        if(pointLevelIndex <= getLevelMinValue("labels"))
                            return "labels";

                        if(pointLevelIndex <= getLevelMinValue("middle"))
                            return "middle";

                        if(pointLevelIndex <= getLevelMinValue("small"))
                            return "small";

                        if(pointLevelIndex <= getLevelMinValue("marks"))
                            return "marks";

                        return "events";
                    }

                    function interpolate(alpha, min, max){ // Find value during animation
                        return min + alpha * (max - min);
                    }


                    var levelIndex = Math.max(scope.scaleManager.levels.marks.index, targetLevels.marks.index); // Target level is lowest of visible

                    // If it this point levelIndex is invisible (less than one pixel per mark) - skip it!
                    var level = RulerModel.levels[levelIndex]; // Actual calculating level
                    var levelDetailizaion = level.interval.getMilliseconds();
                    while( levelDetailizaion < timelineConfig.minMarkWidth * scope.scaleManager.msPerPixel)
                    {
                        levelIndex --;
                        level = RulerModel.levels[levelIndex];
                        levelDetailizaion = level.interval.getMilliseconds();
                    }

                    var start = scope.scaleManager.alignStart(RulerModel.levels[levelIndex>0?(levelIndex-1):0]); // Align start by upper level!
                    var point = start;
                    var end = scope.scaleManager.alignEnd(level);

                    var counter = 0;// Protection from neverending cycles.

                    while(point <= end && counter++ < 3000){
                        var odd = bgColor!= bgOddColor || Math.round((point.getTime() / levelDetailizaion)) % 2 === 1; // add or even for zebra coloring

                        var pointLevelIndex = RulerModel.findBestLevelIndex(point, levelIndex);

                        var levelName = getBestLevelName(pointLevelIndex); // Here we detect best level for this particular point

                        var animation = getPointAnimation(pointLevelIndex, levelName); // We detect, if level is changing
                        var markSize = levelsSettings[levelName].markSize;
                        var transparency = levelsSettings[levelName].transparency;
                        var fontSize = levelsSettings[levelName].fontSize;
                        var labelPositionFix = levelsSettings[levelName].labelPositionFix;

                        if(animation !== null){ //scaling between upperLevel and levelName to up
                            var animationLevelName = getPrevLevelName(pointLevelIndex);
                            markSize = interpolate(animation, markSize, levelsSettings[animationLevelName].markSize);
                            transparency = interpolate(animation, transparency, levelsSettings[animationLevelName].transparency);
                            fontSize = interpolate(animation, fontSize, levelsSettings[animationLevelName].fontSize);
                            labelPositionFix = interpolate(animation, labelPositionFix, levelsSettings[animationLevelName].labelPositionFix);
                        }

                        font.size = fontSize; // Set font size for label

                        //Draw lebel, using calculated values
                        drawLabel(context, point, level, transparency,
                            labelFormat, labelFixed, levelTop, levelHeight,
                            font, labelAlign, odd?bgColor: bgOddColor, markColor, labelPositionFix, markAttach, markSize);

                        point = level.interval.addToDate(point);
                    }
                    if(counter == 3000){
                        console.error("counter problem!", start,point, end, level);
                    }
                }

                function drawLabelsRow (context, currentLevelIndex, taretLevelIndex, animation,
                                        labelFormat, labelFixed, levelTop, levelHeight,
                                        font, labelAlign, bgColor, bgOddColor, markColor, labelPositionFix, markAttach, markHeight){

                    var levelIndex = Math.max(currentLevelIndex, taretLevelIndex);
                    var level = RulerModel.levels[levelIndex]; // Actual calculating level

                    var start1 = scope.scaleManager.alignStart(level);
                    var start = scope.scaleManager.alignStart(level);
                    var end = scope.scaleManager.alignEnd(level);
                    var levelDetailizaion = level.interval.getMilliseconds();


                    var counter = 1000;
                    while(start <= end && counter-- > 0){
                        var odd = Math.round((start.getTime() / levelDetailizaion)) % 2 === 1;
                        var pointLevelIndex = RulerModel.findBestLevelIndex(start);
                        var alpha = 1;
                        if(pointLevelIndex > taretLevelIndex){ //fade out
                            alpha = 1 - animation;
                        }else if(pointLevelIndex > currentLevelIndex){ // fade in
                            alpha =  animation;
                        }
                        drawLabel(context,start,level,alpha,
                                        labelFormat, labelFixed, levelTop, levelHeight, font, labelAlign, odd?bgColor: bgOddColor, markColor, labelPositionFix, markAttach, markHeight);
                        start = level.interval.addToDate(start);
                    }
                    if(counter < 1){
                        console.error("problem!", start1, start, end, level);
                    }
                }
                function drawLabel( context, date, level, alpha,
                                        labelFormat, labelFixed, levelTop, levelHeight, font, labelAlign, bgColor, markColor, labelPositionFix, markAttach, markHeight){

                    var coordinate = scope.scaleManager.dateToScreenCoordinate(date);

                    if(labelFormat) {
                        context.font = formatFont(font);

                        var nextDate = level.interval.addToDate(date);
                        var stopcoordinate = scope.scaleManager.dateToScreenCoordinate(nextDate);
                        var nextlevel = labelFixed?level:RulerModel.findBestLevel(nextDate)
                        var nextLabel = dateFormat(new Date(nextDate), nextlevel[labelFormat]);

                        var bestlevel = labelFixed?level:RulerModel.findBestLevel(date);
                        var label = dateFormat(new Date(date), bestlevel[labelFormat]);
                        var textWidth = context.measureText(label).width;

                        var textStart = levelTop * scope.viewportHeight // Top border
                            + (levelHeight * scope.viewportHeight - font.size) / 2 // Top padding
                            + font.size + labelPositionFix; // Font size

                        var x = 0;
                        switch (labelAlign) {
                            case "center":
                                var leftbound = Math.max(0, coordinate);
                                var rightbound = Math.min(stopcoordinate, scope.viewportWidth);
                                var center = (leftbound + rightbound) / 2;

                                x = scope.scaleManager.bound(
                                        coordinate + timelineConfig.labelPadding,
                                        center - textWidth / 2,
                                        stopcoordinate - textWidth - timelineConfig.labelPadding
                                );

                                break;

                            case "left":
                                x = scope.scaleManager.bound(
                                    timelineConfig.labelPadding,
                                        coordinate + timelineConfig.labelPadding,
                                        stopcoordinate - textWidth - timelineConfig.labelPadding
                                );

                                break;

                            case "above":
                            default:
                                x = coordinate - textWidth / 2;
                                break;
                        }
                    }


                    var markTop = markAttach == "top" ? levelTop : levelTop + levelHeight - markHeight ;

                    if(bgColor) {
                        context.fillStyle = blurColor(bgColor, alpha);

                        switch (labelAlign) {
                            case "center": // fill the whole interval
                            case "left":
                                context.fillRect(coordinate, levelTop * scope.viewportHeight, stopcoordinate - coordinate, levelHeight * scope.viewportHeight); // above
                                break;
                        }
                    }

                    if(markColor) {
                        context.strokeStyle = blurColor(markColor, alpha);
                        context.beginPath();
                        context.moveTo(coordinate + 0.5, markTop * scope.viewportHeight);
                        context.lineTo(coordinate + 0.5, (markTop + markHeight) * scope.viewportHeight);
                        context.stroke();
                    }

                    if(bgColor) {
                        context.fillStyle = blurColor(bgColor, alpha);
                        switch (labelAlign) {
                            case "center": // fill the whole interval
                            case "left":
                                break;
                            case "above":
                            default:
                                context.clearRect(x, textStart - font.size, textWidth, font.size); // above
                                break;
                        }
                    }

                    if(labelFormat) {
                        context.fillStyle = blurColor(font.color, alpha);
                        context.fillText(label, x, textStart);
                    }
                }

                function debugEvents(context){

                    context.fillStyle = blurColor(timelineConfig.chunksBgColor,1);

                    context.clearRect(0, top, scope.viewportWidth , timelineConfig.chunkHeight * scope.viewportHeight);

                    if(scope.recordsProvider && scope.recordsProvider.chunksTree) {

                        var targetLevelIndex = scope.scaleManager.levels.events.index;
                        var events = null;
                        for(var levelIndex=0;levelIndex<RulerModel.levels.length;levelIndex++) {
                            var level = RulerModel.levels[levelIndex];
                            var start = scope.scaleManager.alignStart(level);
                            var end = scope.scaleManager.alignEnd(level);
                            // 1. Splice events
                            var events = scope.recordsProvider.getIntervalRecords(start, end, levelIndex, levelIndex != targetLevelIndex);

                            // 2. Draw em!
                            for (var i = 0; i < events.length; i++) {
                                drawEvent(context, events[i], levelIndex, true, targetLevelIndex);
                            }
                        }


                    }
                }
                // !!! Draw events
                function drawOrCheckEvents(context){
                    var top = (timelineConfig.topLabelHeight + timelineConfig.labelHeight) * scope.viewportHeight; // Top border

                    if(!context){
                        var newMouseInEvents = mouseRow > top && (mouseRow < top + timelineConfig.chunkHeight * scope.viewportHeight);
                        mouseInEvents = newMouseInEvents;
                        return;
                    }

                    context.fillStyle = blurColor(timelineConfig.chunksBgColor,1);

                    context.fillRect(0, top, scope.viewportWidth , timelineConfig.chunkHeight * scope.viewportHeight);

                    var level = scope.scaleManager.levels.events.level;
                    var levelIndex = scope.scaleManager.levels.events.index;
                    var start = scope.scaleManager.alignStart(level);
                    var end = scope.scaleManager.alignEnd(level);


                    if(scope.recordsProvider && scope.recordsProvider.chunksTree) {
                        // 1. Splice events
                        var events = scope.recordsProvider.getIntervalRecords(start, end, levelIndex);

                        // 2. Draw em!
                        for(var i=0;i<events.length;i++){
                            drawEvent(context,events[i],levelIndex);
                        }
                    }

                }
                function drawEvent(context,chunk, levelIndex, debug, targetLevelIndex){
                    var startCoordinate = scope.scaleManager.dateToScreenCoordinate(chunk.start);
                    var endCoordinate = scope.scaleManager.dateToScreenCoordinate(chunk.end);

                    var blur = 1;//chunk.level/(RulerModel.levels.length - 1);

                    context.fillStyle = blurColor(timelineConfig.exactChunkColor,blur); //green

                    var top = (timelineConfig.topLabelHeight + timelineConfig.labelHeight) * scope.viewportHeight;
                    var height = timelineConfig.chunkHeight * scope.viewportHeight;

                    if(debug){
                        if(levelIndex == chunk.level) { // not exact chunk
                            context.fillStyle = blurColor(timelineConfig.loadingChunkColor,blur); //blue
                        }

                        if(!chunk.level){ //blind spot!
                            context.fillStyle = blurColor(timelineConfig.blindChunkColor,1); // red
                        }

                        if(targetLevelIndex == levelIndex) {
                            context.fillStyle = blurColor(timelineConfig.highlighChunkColor, 1); //yellow
                        }

                        top += (1+levelIndex) / RulerModel.levels.length * height;
                        height /= RulerModel.levels.length;

                        top = Math.floor(top);
                        height = Math.ceil(height);
                    }

                    context.fillRect(startCoordinate - timelineConfig.minChunkWidth/2,
                        top,
                        (endCoordinate - startCoordinate) + timelineConfig.minChunkWidth/2,
                        height);
                }


                var lastMinuteTexture = false;
                var lastMinuteTextureImg = false;
                    function drawLastMinute(context){
                    // 1. Get start coordinate for last minute
                    var end = scope.scaleManager.end;
                    var start = scope.scaleManager.lastMinute();


                    if(start >= scope.scaleManager.visibleEnd){
                        return; // Do not draw - just skip it
                    }
                    // 2. Get texture

                    if(lastMinuteTexture){
                        context.fillStyle = lastMinuteTexture;
                    }else{
                        if(!lastMinuteTextureImg) {
                            var img = new Image();
                            img.onload = function () {
                                lastMinuteTexture = context.createPattern(img, 'repeat');
                                context.fillStyle = lastMinuteTexture;
                            };
                            img.src = 'images/lastminute.png';
                            lastMinuteTextureImg = img;
                        }
                        context.fillStyle = blurColor(timelineConfig.lastMinuteColor,1);
                    }

                    // 3. Draw last minute with texture
                    var startCoordinate = scope.scaleManager.dateToScreenCoordinate(start);
                    var endCoordinate = scope.scaleManager.dateToScreenCoordinate(end);

                    var top = (timelineConfig.topLabelHeight + timelineConfig.labelHeight) * scope.viewportHeight;
                    var height = timelineConfig.chunkHeight * scope.viewportHeight;

                    var offset_x = - (start / timelineConfig.lastMinuteAnimationMs) % timelineConfig.lastMinuteTextureSize;

                    context.save();
                    context.translate(offset_x, 0);

                    context.fillRect(startCoordinate - timelineConfig.minChunkWidth/2 - offset_x,
                        top,
                            (endCoordinate - startCoordinate) + timelineConfig.minChunkWidth/2,
                        height);

                    context.restore();
                }

                var scrollBarWidth = 0;
                // !!! Draw ScrollBar
                function drawOrCheckScrollBar(context){
                    var top = (timelineConfig.topLabelHeight + timelineConfig.labelHeight + timelineConfig.chunkHeight) * scope.viewportHeight; // Top border

                    //2.
                    var relativeCenter =  scope.scaleManager.getRelativeCenter();
                    var relativeWidth =  scope.scaleManager.getRelativeWidth();

                    scrollBarWidth = Math.max(scope.viewportWidth * relativeWidth, timelineConfig.minScrollBarWidth);
                    // Correction for width if it has minimum width
                    var startCoordinate = scope.scaleManager.bound( 0, (scope.viewportWidth * relativeCenter - scrollBarWidth/2), scope.viewportWidth - scrollBarWidth) ;

                    mouseInScrollbarRow = mouseRow >= top;
                    mouseInScrollbar = mouseCoordinate >= startCoordinate && mouseCoordinate <= startCoordinate + scrollBarWidth && mouseRow >= top;
                    if(context) {
                        //1. DrawBG
                        context.fillStyle = blurColor(timelineConfig.scrollBarBgColor, 1);
                        context.fillRect(0, top, scope.viewportWidth, timelineConfig.scrollBarHeight * scope.viewportHeight);

                        //2. drawOrCheckScrollBar
                        context.fillStyle = (mouseInScrollbar || catchScrollBar) ? blurColor(timelineConfig.scrollBarHighlightColor, 1) : blurColor(timelineConfig.scrollBarColor, 1);
                        context.fillRect(startCoordinate, top, scrollBarWidth, timelineConfig.scrollBarHeight * scope.viewportHeight);
                    }else{
                        if(mouseInScrollbar || catchScrollBar){
                            mouseInScrollbar = mouseCoordinate - startCoordinate;
                            mouseInTimeline = false;
                        }else{
                            mouseInTimeline = mouseCoordinate;
                        }
                        if(mouseInScrollbarRow){
                            mouseInScrollbarRow = mouseCoordinate - startCoordinate;
                        }
                        return mouseInScrollbar;
                    }
                }

                // !!! Draw and position for timeMarker
                function drawTimeMarker(context){
                    if(!scope.positionProvider || scope.positionProvider.liveMode){
                        return;
                    }
                    var lastMinute = scope.scaleManager.lastMinute();
                    if(scope.positionProvider.playedPosition > lastMinute && scope.positionProvider.playing){
                        scope.goToLive();
                    }
                    drawMarker(context, scope.positionProvider.playedPosition, timelineConfig.timeMarkerColor, timelineConfig.timeMarkerTextColor)
                }

                function drawPointerMarker(context){
                    if(window.jscd.mobile || !mouseCoordinate || !mouseInEvents){
                        return;
                    }

                    drawMarker(context, scope.scaleManager.screenCoordinateToDate(mouseCoordinate),timelineConfig.pointerMarkerColor,timelineConfig.pointerMarkerTextColor);
                }

                function drawMarker(context, date, markerColor, textColor){
                    var coordinate =  scope.scaleManager.dateToScreenCoordinate(date);

                    if(coordinate < 0 || coordinate > scope.viewportWidth ) {
                        return;
                    }

                    date = new Date(date);

                    var height = timelineConfig.markerHeight * scope.viewportHeight;

                    // Line
                    context.strokeStyle = blurColor(markerColor,1);
                    context.fillStyle = blurColor(markerColor,1);

                    var top = (timelineConfig.topLabelHeight + timelineConfig.labelHeight) * scope.viewportHeight;

                    context.beginPath();
                    context.moveTo(coordinate + 0.5, top);
                    context.lineTo(coordinate + 0.5, Math.round(scope.viewportHeight - timelineConfig.scrollBarHeight * scope.viewportHeight));
                    context.stroke();

                    var startCoord = coordinate - timelineConfig.markerWidth /2;
                    if(startCoord < 0){
                        startCoord = 0;
                    }
                    if(startCoord > scope.viewportWidth - timelineConfig.markerWidth){
                        startCoord = scope.viewportWidth - timelineConfig.markerWidth;
                    }

                    // Bubble
                    context.fillRect(startCoord, 0, timelineConfig.markerWidth, height );

                    // Triangle
                    context.beginPath();
                    context.moveTo(coordinate + timelineConfig.markerTriangleHeight * scope.viewportHeight + 0.5, height);
                    context.lineTo(coordinate + 0.5, height + timelineConfig.markerTriangleHeight * scope.viewportHeight);
                    context.lineTo(coordinate - timelineConfig.markerTriangleHeight * scope.viewportHeight + 0.5, height);
                    context.closePath();
                    context.fill();

                    // Labels
                    context.fillStyle = blurColor(textColor,1);
                    context.font = formatFont(timelineConfig.markerDateFont);
                    coordinate = startCoord + timelineConfig.markerWidth /2; // Set actual center of the marker

                    var dateString = debugTime? date.getTime() : dateFormat(date, timelineConfig.dateFormat);
                    var dateWidth = context.measureText(dateString).width;
                    var textStart = (height - timelineConfig.markerDateFont.size) / 2;
                    context.fillText(dateString,coordinate - dateWidth/2, textStart);


                    context.font = formatFont(timelineConfig.markerTimeFont);
                    dateString = dateFormat(date, timelineConfig.timeFormat);
                    dateWidth = context.measureText(dateString).width;
                    textStart = height/2 + textStart;
                    context.fillText(dateString,coordinate - dateWidth/2, textStart);

                }


                var mouseOverRightScrollButton = false;
                var mouseOverLeftScrollButton = false;

                function drawOrCheckLeftRightButtons(context){

                    var canScrollRight = scope.scaleManager.canScroll(false);
                    var canScrollLeft = scope.scaleManager.canScroll(true);

                    mouseOverRightScrollButton = canScrollRight && mouseCoordinate > scope.viewportWidth - timelineConfig.leftRightButtonsWidth;
                    mouseOverLeftScrollButton = canScrollLeft && mouseCoordinate < timelineConfig.leftRightButtonsWidth;

                    //console.log("drawOrCheckLeftRightButtons",canScrollRight,canScrollLeft,mouseOverRightScrollButton,mouseOverLeftScrollButton);

                    if(!context){ return; }

                    if(canScrollLeft ) {
                        drawScrollButton(context, true, mouseOverLeftScrollButton);
                    }
                    if(canScrollRight) {
                        drawScrollButton(context, false, mouseOverRightScrollButton);
                    }
                }

                function drawScrollButton(context, left, active){

                    context.fillStyle = active? blurColor(timelineConfig.leftRightButtonsActiveColor,1):blurColor(timelineConfig.leftRightButtonsColor,1);

                    var startCoord = left?0: scope.viewportWidth - timelineConfig.leftRightButtonsWidth;
                    var height = timelineConfig.leftRightButtonsHeight * scope.viewportHeight;

                    context.fillRect(startCoord, scope.viewportHeight - height, timelineConfig.leftRightButtonsWidth, height );

                    var caption = left?'<':'>';

                    context.lineWidth = timelineConfig.leftRightButtonsArrowLineWidth;
                    context.strokeStyle =  active? blurColor(timelineConfig.leftRightButtonsArrowActiveColor,1):blurColor(timelineConfig.leftRightButtonsArrowColor,1);

                    var leftCoord = startCoord + ( timelineConfig.leftRightButtonsWidth - timelineConfig.leftRightButtonsArrow.width)/2;
                    var rightCoord = leftCoord + timelineConfig.leftRightButtonsArrow.width;

                    var topCoord = scope.viewportHeight - (height + timelineConfig.leftRightButtonsArrow.size) / 2;
                    var bottomCoord = topCoord + timelineConfig.leftRightButtonsArrow.size;


                    context.beginPath();
                    context.moveTo(left?rightCoord:leftCoord, topCoord);
                    context.lineTo(left?leftCoord:rightCoord, (topCoord + bottomCoord)/2);
                    context.lineTo(left?rightCoord:leftCoord, bottomCoord);
                    context.stroke();

                }

                // !!! Mouse events
                var mouseCoordinate = null;
                var mouseRow = 0;
                var mouseInScrollbar = false;
                var mouseInEvents = false;
                var mouseInTimeline = false;
                var mouseInScrollbarRow = false;
                var catchScrollBar = false;
                var catchTimeline = false;

                function updateMouseCoordinate( event){
                    if(!event){
                        mouseRow = 0;
                        mouseCoordinate = null;
                        mouseInScrollbar = false;
                        mouseInScrollbarRow = false;
                        mouseInEvents = false;
                        mouseInTimeline = false;
                        return;
                    }

                    if($(event.target).is("canvas") && event.offsetX){
                        mouseCoordinate = event.offsetX;
                    }else{
                        mouseCoordinate = event.pageX - $(canvas).offset().left;
                    }
                    mouseRow = event.offsetY || (event.pageY - $(canvas).offset().top);

                    drawOrCheckScrollBar();
                    if( mouseCoordinate >= 0 ) {
                        drawOrCheckLeftRightButtons();
                        drawOrCheckEvents();
                    }

                }

                var zoomTarget = 0;
                function mouseWheel(event){
                    updateMouseCoordinate(event);
                    event.preventDefault();
                    if(Math.abs(event.deltaY) > Math.abs(event.deltaX)) { // Zoom or scroll - not both
                        if(window.jscd.touch ) {
                            zoomTarget = scope.scaleManager.zoom() - event.deltaY / timelineConfig.maxVerticalScrollForZoomWithTouch;
                        }else{
                            // We need to smooth zoom here
                            // Collect zoom changing in zoomTarget
                            if(!zoomTarget) {
                                zoomTarget = scope.scaleManager.zoom();
                            }
                            zoomTarget -= event.deltaY / timelineConfig.maxVerticalScrollForZoom;
                            zoomTarget = scope.scaleManager.boundZoom(zoomTarget);
                        }
                        scope.zoomTo(zoomTarget, scope.scaleManager.screenCoordinateToDate(mouseCoordinate),window.jscd.touch);
                    } else {
                        scope.scaleManager.scrollByPixels(event.deltaX);
                        delayWatchingPlayingPosition();
                    }
                    scope.$apply();
                }

                var stopDelay = null;
                function delayWatchingPlayingPosition(){
                    if(!stopDelay) {
                        scope.scaleManager.stopWatching();
                    }else{
                        clearTimeout(stopDelay);
                    }
                    stopDelay = setTimeout(function(){
                        scope.scaleManager.releaseWatching();
                        stopDelay = null;
                    },timelineConfig.animationDuration);
                }

                function animateScroll(targetPosition,slow){
                    delayWatchingPlayingPosition();

                    animateScope.animate(scope,"scrollTarget",targetPosition,slow?"linear":false).
                        then(
                            function(){},
                            function(){},
                            function(value){
                                scope.scaleManager.scroll(value);
                            }
                        );
                }


                var preventClick = false;



                var scrollingNow = false;
                var scrollingLeft = false;
                function setScroll(value){
                    scope.scaleManager.scroll(value);
                }
                function processScrolling(){
                    if(scrollingNow) {
                        var moveScroll = (scrollingLeft ? -1 : 1) * timelineConfig.slowScrollSpeed * scope.viewportWidth;
                        var scrollTarget = scope.scaleManager.getScrollByPixelsTarget(moveScroll);
                        animateScroll(scrollTarget,true);
                    }
                }
                scope.stopScroll = function(left) {
                    if(scrollingNow) {
                        scrollingNow = false;
                    }
                };
                scope.startScroll = function(left) {

                    console.log("startScroll");
                    if(!scope.scaleManager.canScroll(left)){
                        return;
                    }

                    scope.scrollTarget = scope.scaleManager.scroll();

                    scrollingNow = true;
                    scrollingLeft = left;

                    processScrolling();
                };
                scope.scrollStep = function(left){
                    scope.scrollTarget = scope.scaleManager.scroll();
                    var moveScroll = (left ? -1 : 1) * timelineConfig.scrollSpeed * scope.viewportWidth;
                    var scrollTarget = scope.scaleManager.getScrollByPixelsTarget(moveScroll);
                    animateScroll(scrollTarget);
                };

                scope.dblClick = function(event){
                    updateMouseCoordinate(event);
                    if(preventClick){
                        return;
                    }

                    if(mouseOverLeftScrollButton){
                        animateScroll(0);
                        return;
                    }

                    if(mouseOverRightScrollButton){
                        animateScroll(1);
                        return;
                    }

                    if(!mouseInScrollbarRow) {
                        scope.scaleManager.setAnchorCoordinate(mouseCoordinate);// Set position to keep
                        scope.zoom(true);
                    }else{
                        if(!mouseInScrollbar){
                            animateScroll((mouseInScrollbarRow>0?1:0));
                        }
                    }


                };
                scope.click = function(event){
                    updateMouseCoordinate(event);
                    if(preventClick){
                        return;
                    }

                    if(mouseOverRightScrollButton){
                        scope.scrollStep(false);
                        //Scroll by percents
                        return;
                    }
                    if(mouseOverLeftScrollButton){
                        scope.scrollStep(true);
                        //Scroll by percents
                        return;
                    }

                    if(!mouseInScrollbarRow) {
                        scope.scaleManager.setAnchorCoordinate(mouseCoordinate);// Set position to keep
                        var date = scope.scaleManager.screenCoordinateToDate(mouseCoordinate);

                        var lastMinute = scope.scaleManager.lastMinute();
                        if(date > lastMinute){
                            scope.goToLive();
                        }else {
                            scope.positionHandler(date);
                            scope.scaleManager.watchPlaying(date);
                        }

                    }else{
                        if(!mouseInScrollbar){
                            scope.scrollPosition = scope.scaleManager.scroll() ;
                            animateScroll(mouseCoordinate / scope.viewportWidth);
                        }
                    }
                };

                scope.mouseUp = function(event){


                    scope.stopScroll(false);
                    //updateMouseCoordinate(event);
                    //TODO: "set timer for dblclick here";
                    //TODO: "move playing position";
                };
                scope.mouseLeave = function(event){
                    mouseInEvents = false;
                    mouseInTimeline = false;
                    //updateMouseCoordinate(null);
                };
                scope.mouseMove = function(event){
                    updateMouseCoordinate(event);
                    /*if(catchScrollBar){
                        var moveScroll = mouseInScrollbar - catchScrollBar;
                        scope.scaleManager.scroll( scope.scaleManager.scroll() + moveScroll / scope.viewportWidth );
                    }*/
                };
                scope.mouseDown = function(event){


                };

                scope.draginit = function(event){
                    updateMouseCoordinate(event);

                    if(mouseOverLeftScrollButton){
                        scope.startScroll(true);
                        return;
                    }

                    if(mouseOverRightScrollButton){
                        scope.startScroll(false);
                        return;
                    }

                    catchScrollBar = mouseInScrollbar;
                    catchTimeline = mouseInTimeline;

                    if(catchTimeline && catchScrollBar) {
                        scope.scaleManager.stopWatching();
                    }
                };
                scope.drag = function(event){
                    updateMouseCoordinate(event);

                    if(catchScrollBar) {
                        var moveScroll = mouseInScrollbar - catchScrollBar;
                        scope.scaleManager.scroll(scope.scaleManager.scroll() + moveScroll / scope.viewportWidth);
                    }
                    if(catchTimeline) {
                        var moveScroll = catchTimeline - mouseInTimeline;
                        catchTimeline = mouseInTimeline;
                        scope.scaleManager.scrollByPixels(moveScroll);
                    }
                };
                scope.drastart = function(event){
                    console.log("drastart");
                };
                scope.dragend = function(event){
                    catchScrollBar = false;
                    catchTimeline = false;
                    scope.scaleManager.releaseWatching();

                    preventClick = true;
                    setTimeout(function(){
                        preventClick = false;
                    }, timelineConfig.animationDuration);
                    updateMouseCoordinate(null);
                };

                $(canvas).drag("draginit",scope.draginit);
                $(canvas).drag("dragstart",scope.drastart);
                $(canvas).drag("drag",scope.drag);
                $(canvas).drag("dragend",scope.dragend);

                $(canvas).bind("touchstart", scope.draginit);
                $(canvas).bind("touchmove", scope.drag);
                $(canvas).bind("touchend", scope.dragend);
                $(canvas).bind("touchcancel",scope.dragend);


                // !!! Scrolling functions


                // !!! Zooming function
                scope.zoom = function(zoomIn,slow,slowAnimation) {
                    zoomTarget = scope.scaleManager.zoom() - (zoomIn ? 1 : -1) * (slow?timelineConfig.slowZoomSpeed:timelineConfig.zoomSpeed);
                    scope.zoomTo(zoomTarget,null,false,slowAnimation);
                };
                scope.zoomOut = function(){
                    zoomTarget = scope.scaleManager.boundZoom(1);
                    scope.zoomTo(zoomTarget);
                };

                var zoomingNow = false;
                var zoomingIn = false;
                function processZooming(){ // renew zooming
                    if(zoomingNow) {
                        scope.zoom(zoomingIn,true,false);
                    }
                }
                scope.stopZoom = function(zoomIn) {
                    if(zoomingNow) {
                        zoomingNow = false;
                        scope.zoom(zoomingIn, true, true);
                    }
                };
                scope.startZoom = function(zoomIn) {
                    if(scope.disableZoomOut&&!zoomIn || scope.disableZoomIn&&zoomIn){
                        return;
                    }

                    zoomingNow = true;
                    zoomingIn = zoomIn;

                    processZooming();
                };
                scope.zooming = 1; // init animation value
                function levelsChanged(newLevels,oldLevels){
                    if(newLevels.labels.index != oldLevels.labels.index) {
                        return true;
                    }

                    if(newLevels.middle.index != oldLevels.middle.index) {
                        return true;
                    }

                    if(newLevels.small.index != oldLevels.small.index) {
                        return true;
                    }

                    if(newLevels.marks.index != oldLevels.marks.index) {
                        return true;
                    }

                    return false;
                }
                function checkZoomButtons(){
                    scope.disableZoomOut = !scope.scaleManager.çheckZoomOut();
                    scope.disableZoomIn =  !scope.scaleManager.çheckZoomIn();
                }
                scope.zoomTo = function(zoomTarget, zoomDate, instant, slow){

                    zoomTarget = scope.scaleManager.boundZoom(zoomTarget);


                    //Find final levels for this zoom and run animation:
                    var newTargetLevels = scope.scaleManager.targetLevels(zoomTarget);
                    if(levelsChanged(newTargetLevels, targetLevels)){
                        targetLevels = newTargetLevels;

                        if( scope.zooming == 1){ // We need to run animation again
                            scope.zooming = 0;
                        }

                        // This allows us to continue (and slowdown, mb) animation every time
                        animateScope.animate(scope,"zooming",1,"dryResistance").then(function(){
                            currentLevels = scope.scaleManager.levels;
                        },function(){
                            // ignore animation re-run
                        });
                    }


                    function setZoom(value){
                        if (zoomDate) {
                            scope.scaleManager.zoomAroundDate(
                                value,
                                zoomDate
                            );
                        } else {
                            scope.scaleManager.zoom(value);
                        }
                        $timeout(checkZoomButtons);
                        delayWatchingPlayingPosition();
                    }


                    if(!instant) {
                        if(!scope.zoomTarget) {
                            scope.zoomTarget = scope.scaleManager.zoom();
                        }

                        delayWatchingPlayingPosition();
                        animateScope.animate(scope, "zoomTarget", zoomTarget, slow?"linear":"dryResistance").then(
                            function () {},
                            function () {},
                            setZoom);
                    }else{
                        setZoom(zoomTarget);
                        scope.zoomTarget = scope.scaleManager.zoom();
                    }
                };




                scope.goToLive = function(force){
                    var moveDate = scope.scaleManager.screenCoordinateToDate(1);
                    animateScope.progress(scope, "goingToLive" ).then(
                        function(){
                            var activeDate = (new Date()).getTime();
                            scope.scaleManager.setAnchorDateAndPoint(activeDate,1);
                            scope.scaleManager.watchPlaying();
                        },
                        function(){},
                        function(val){
                            var activeDate = moveDate + val * ((new Date()).getTime() - moveDate);
                            scope.scaleManager.setAnchorDateAndPoint(activeDate,1);
                        });

                    if(!scope.positionProvider.liveMode || force) {
                        scope.positionHandler(false);
                    }
                };

                scope.playPause = function(){
                    if(!scope.positionProvider.playing){
                        scope.scaleManager.watchPlaying();
                    }

                    scope.playHandler(!scope.positionProvider.playing);
                };

                // !!! Subscribe for different events which affect timeline
                $( window ).resize(updateTimelineWidth);    // Adjust width after window was resized

                scope.$watch("positionProvider.liveMode",function(mode){
                    if(scope.positionProvider && scope.positionProvider.liveMode) {
                        scope.goToLive(true);
                    }
                });
                scope.$watch('recordsProvider',function(){ // RecordsProvider was changed - means new camera was selected
                    if(scope.recordsProvider) {
                        scope.recordsProvider.ready.then(initTimeline);// reinit timeline here
                        scope.recordsProvider.archiveReadyPromise.then(function(hasArchive){
                            scope.emptyArchive = !hasArchive;
                        });
                    }
                });
                viewport.mousewheel(mouseWheel);

                // !!! Finally run required functions to initialize timeline
                updateTimelineHeight();
                updateTimelineWidth(); // Adjust width
                initTimeline(); // Setup boundaries and scale

                // !!! Start drawing
                animateScope.setDuration(timelineConfig.animationDuration);
                animateScope.setScope(scope);
                animateScope.start(drawAll);
                scope.$on('$destroy', function() { animateScope.stop(); });
            }
        };
    }]);

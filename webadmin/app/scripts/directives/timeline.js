'use strict';

angular.module('webadminApp')
    .directive('timeline', ['$interval','$timeout','animateScope', function ($interval,$timeout,animateScope) {
        return {
            restrict: 'E',
            scope: {
                recordsProvider: '=',
                positionProvider: '=',
                playHandler: '=',
                ngClick: '&',
                positionHandler: '='
            },
            templateUrl: 'views/components/timeline.html',
            link: function (scope, element/*, attrs*/) {
                var timelineConfig = {
                    initialInterval: 1000*60 *60*24*365 /* *24*365*/, // no records - show small interval
                    maxMsPerPixel: 1000*60*60*24*365,   // one year per pixel - maximum view
                    minMsPerPixel: 1, // Minimum level for zooming:

                    zoomSpeed: 0.025, // Zoom speed for buttons 0 -> 1 = full zoom
                    maxVerticalScrollForZoom: 50, // value for adjusting zoom
                    maxVerticalScrollForZoomWithTouch: 5000, // value for adjusting zoom
                    animationDuration: 500, // 300, // 200-400 for smooth animation

                    timelineBgColor: [28,35,39], //Color for ruler marks and labels
                    font:'Roboto',//'sans-serif',

                    labelPadding: 10,
                    lineWidth: 1,

                    chunkHeight:35/110, // %    //Height for event line
                    minChunkWidth: 1,
                    chunksBgColor:[34,57,37],
                    exactChunkColor: [58,145,30],
                    loadingChunkColor: [58,145,30],

                    scrollBarSpeed: 0.1, // By default - scroll by 10%
                    scrollBarHeight: 20/110, // %
                    minScrollBarWidth: 50, // px
                    scrollBarBgColor: [28,35,39],
                    scrollBarColor: [53,70,79],
                    scrollBarHighlightColor: [53,70,79,0.8],

                    timeMarkerColor: [255,255,255], // Timemarker color
                    timeMarkerTextColor: [0,0,0],
                    pointerMarkerColor: [0,0,0], // Mouse pointer marker color
                    pointerMarkerTextColor: [255,255,255],
                    markerFont:{
                        size:12,
                        weight:400,
                        face:"Roboto"
                    },
                    markerWidth: 120,
                    markerHeight: 45/110,
                    markerTriangleHeight: 10/110,
                    dateFormat: 'd mmmm yyyy', // Timemarker format for date
                    timeFormat: 'HH:MM:ss', // Timemarker format for time

                    scrollBoundariesPrecision: 0.000001, // Where we should disable right and left scroll buttons
                    scrollingSpeed: 0.25,// Scrolling speed for scroll buttons right-left

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


                    labelAlign:"above",// center, left, above
                    labelHeight:25/110, // %
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


                    lowerMarkerAttach:"bottom", // top, bottom
                    lowerMarksHeight:10/110,
                    lowerMarksColor:[83,112,127,0.4]
                };


                scope.presets = [{
                    topLabelAlign: "center", // center, left, above
                    topLabelMarkerColor: [105,135,150], // Color for mark for top label
                    topLabelBgColor: false,
                    topLabelBgOddColor: false,

                    labelAlign:"above",// center, left, above
                    labelMarkerColor:[53,70,79,0],//false
                    labelBgColor: [28,35,39],

                    lowerMarksColor:[83,112,127,0]
                },{
                    topLabelAlign: "center", // center, left, above
                    topLabelMarkerColor: [105,135,150], // Color for mark for top label
                    topLabelBgColor: false,
                    topLabelBgOddColor: false,

                    labelAlign:"above",// center, left, above
                    labelMarkerColor:[83,112,127,0.6],//false
                    labelBgColor: [28,35,39],

                    lowerMarksColor:[83,112,127,0]
                },{
                    topLabelAlign: "left", // center, left, above
                    topLabelMarkerColor: [105,135,150], // Color for mark for top label
                    topLabelBgColor: false,
                    topLabelBgOddColor: false,

                    labelAlign:"left",// center, left, above
                    labelMarkerColor:[83,112,127,0.6],//false
                    labelBgColor: [28,35,39],

                    lowerMarksColor:[83,112,127,0.4]
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
                    topLabelPositionFix:0, //Vertical fix for position


                    labelAlign:"above",// center, left, above
                    labelHeight:35/110, // %
                    labelFont:{
                        size:15,
                        weight:400,
                        face:"Roboto",
                        color: [105,135,150] // Color for text for top labels
                    },
                    labelMarkerAttach:"top", // top, bottom
                    labelMarkerColor:[83,112,127,0.6],//false
                    labelBgColor: [28,35,39],
                    labelPositionFix:5, //Vertical fix for position
                    labelMarkerHeight:20/110,

                    lowerMarkerAttach:"top", // top, bottom
                    lowerMarksHeight:10/110,
                    lowerMarksColor:[83,112,127,0.4]
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
                var timeMarker = element.find(".timeMarker.playing");
                var pointerMarker = element.find(".timeMarker.pointer");
                var canvas = element.find('canvas').get(0);
                scope.scaleManager = new ScaleManager( timelineConfig.minMsPerPixel,timelineConfig.maxMsPerPixel, timelineConfig.initialInterval, 100); //Init boundariesProvider

                // !!! Initialization functions
                function updateTimelineHeight(){
                    scope.viewportHeight = element.find(".viewport").height();
                    canvas.height = scope.viewportHeight;
                }
                function updateTimelineWidth(){
                    scope.viewportWidth = element.find(".viewport").width();
                    canvas.width  = scope.viewportWidth;
                    scope.scaleManager.setViewportWidth(scope.viewportWidth);
                }
                function initTimeline(){
                    var now = (new Date()).getTime();
                    scope.scaleManager.setStart(scope.recordsProvider ? scope.recordsProvider.start : (now - timelineConfig.initialInterval));
                    scope.scaleManager.setEnd(now);
                    scope.scaleManager.fullZoomOut();
                }

                // !!! Drawing and redrawing functions
                function drawAll(){
                    scope.scaleManager.setEnd((new Date()).getTime()); // Set right border
                    if(scope.positionProvider) {
                        scope.scaleManager.tryToSetLiveDate(scope.positionProvider.playedPosition);
                    }

                    var context = clearTimeline();
                    drawTopLabels(context);
                    drawLabels(context);
                    drawLowerMarks(context);

                    drawEvents(context);
                    drawOrCheckScrollBar(context);
                    drawTimeMarker(context);
                    drawPointerMarker(context);
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
                    return font.weight + " " + font.size + "px " + font.face;
                }

                function clearTimeline(){
                    var context = canvas.getContext('2d');
                    context.fillStyle = blurColor(timelineConfig.timelineBgColor,1);
                    context.fillRect(0, 0, scope.viewportWidth, scope.viewportHeight);
                    context.lineWidth = timelineConfig.lineWidth;
                    return context;
                }

                // !!! Labels logic
                var currentTopLabelLevelIndex = 0;
                var targetTopLabelLevelIndex = 0;
                scope.changingTopLevel = 1;
                function drawTopLabels(context){
                    var instantLevelIndex = scope.scaleManager.levels.topLabels.index;
                    if(instantLevelIndex != targetTopLabelLevelIndex){ // Start animation here
                        targetTopLabelLevelIndex = instantLevelIndex;
                        animateScope.progress(scope,'changingTopLevel').then(function(){
                            currentTopLabelLevelIndex = targetTopLabelLevelIndex;
                        });
                    }

                    drawLabelsRow(context,
                        currentTopLabelLevelIndex,
                        targetTopLabelLevelIndex,
                        scope.changingTopLevel,
                        "topFormat",
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

                var currentLabelLevelIndex = 0;
                var targetLabelLevelIndex = 0;
                scope.changingLevel = 1;
                function drawLabels(context){
                    var instantLevelIndex = scope.scaleManager.levels.labels.index;
                    if(instantLevelIndex != targetLabelLevelIndex){ // Start animation here
                        targetLabelLevelIndex = instantLevelIndex;
                        animateScope.progress(scope,'changingLevel').then(function(){
                            currentLabelLevelIndex = targetLabelLevelIndex;
                        });
                    }


                    drawLabelsRow(
                        context,
                        currentLabelLevelIndex,
                        targetLabelLevelIndex,
                        scope.changingLevel,
                        "format",
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

                }


                var currentLowerMarksLevelIndex = 0;
                var targetLowerMarksLevelIndex = 0;
                scope.changingLowerMarksLevel = 1;
                function drawLowerMarks(context){
                    var instantLevelIndex = scope.scaleManager.levels.lowerMarks.index;
                    if(instantLevelIndex != targetLowerMarksLevelIndex){ // Start animation here
                        targetLowerMarksLevelIndex = instantLevelIndex;
                        animateScope.progress(scope,'changingLowerMarksLevel').then(function(){

                            currentLowerMarksLevelIndex = targetLowerMarksLevelIndex;
                        });
                    }

                    drawLabelsRow(
                        context,
                        currentLowerMarksLevelIndex,
                        targetLowerMarksLevelIndex,
                        scope.changingLowerMarksLevel,
                        false,// No format
                        timelineConfig.topLabelHeight,
                        timelineConfig.labelHeight,
                        null,//font
                        null,//align
                        null,//no bg label
                        null,//no bg label
                        timelineConfig.lowerMarksColor,
                        0,
                        timelineConfig.lowerMarkerAttach,
                        timelineConfig.lowerMarksHeight );
                }

                function drawLabelsRow (context, currentLevelIndex, taretLevelIndex, animation,
                                        labelFormat, levelTop, levelHeight,
                                        font, labelAlign, bgColor, bgOddColor, markColor, labelPositionFix, markAttach, markHeight){

                    var levelIndex = Math.max(currentLevelIndex, taretLevelIndex);
                    var level = RulerModel.levels[levelIndex]; // Actual calculating level

                    var start1 = scope.scaleManager.alignStart(level);
                    var start = scope.scaleManager.alignStart(level);
                    var end = scope.scaleManager.alignEnd(level);

                    function checkDate(date){
                        return !isNaN( date.getTime());
                    }
                    if(!(checkDate(start)|| checkDate(end))){
                        console.error("broken Date",checkDate(start), checkDate(end));
                    }

                    var counter = 1000;
                    while(start <= end && counter-- > 0){
                        var odd = Math.round((start.getTime() / level.interval.getMilliseconds())) % 2 === 1;
                        var pointLevelIndex = RulerModel.findBestLevelIndex(start);
                        var alpha = 1;
                        if(pointLevelIndex > taretLevelIndex){ //fade out
                            alpha = 1 - animation;
                        }else if(pointLevelIndex > currentLevelIndex){ // fade in
                            alpha =  animation;
                        }
                        drawLabel(context,start,level,alpha,
                                        labelFormat, levelTop, levelHeight, font, labelAlign, odd?bgColor: bgOddColor, markColor, labelPositionFix, markAttach, markHeight);
                        var newStart = level.interval.addToDate(start);

                        if(!(checkDate(start)|| checkDate(newStart))){
                            console.error("broken Date 2 ",checkDate(start), checkDate(newStart));
                        }
                        start = newStart;
                    }
                    if(counter < 1){
                        console.error("problem!", start1, start, end, level);
                    }
                }
                function drawLabel( context, date, level, alpha,
                                        labelFormat, levelTop, levelHeight, font, labelAlign, bgColor, markColor, labelPositionFix, markAttach, markHeight){

                    var coordinate = scope.scaleManager.dateToScreenCoordinate(date);

                    if(labelFormat) {
                        context.font = formatFont(font);

                        var nextDate = level.interval.addToDate(date);
                        var stopcoordinate = scope.scaleManager.dateToScreenCoordinate(nextDate);
                        var nextLabel = dateFormat(new Date(nextDate), level[labelFormat]);
                        var nextWidth = context.measureText(nextLabel).width;

                        var label = dateFormat(new Date(date), level[labelFormat]);
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
                                x = scope.scaleManager.bound(
                                    timelineConfig.labelPadding,
                                        coordinate - textWidth / 2,
                                        stopcoordinate - nextWidth / 2 - textWidth - timelineConfig.labelPadding
                                );
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
                                context.fillRect(x, textStart - font.size, textWidth, font.size); // above
                                break;
                        }
                    }

                    if(labelFormat) {
                        context.fillStyle = blurColor(font.color, alpha);
                        context.fillText(label, x, textStart);
                    }
                }

                // !!! Draw events
                function drawEvents(context){
                    context.fillStyle =   blurColor(timelineConfig.chunksBgColor,1);
                    var top = (timelineConfig.topLabelHeight + timelineConfig.labelHeight) * scope.viewportHeight; // Top border
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
                function drawEvent(context,chunk, levelIndex){
                    var startCoordinate = scope.scaleManager.dateToScreenCoordinate(chunk.start);
                    var endCoordinate = scope.scaleManager.dateToScreenCoordinate(chunk.end);

                    var exactChunk = levelIndex == chunk.level;

                    context.fillStyle = exactChunk? blurColor(timelineConfig.exactChunkColor,1):blurColor(timelineConfig.loadingChunkColor,1);

                    var top = (timelineConfig.topLabelHeight + timelineConfig.labelHeight) * scope.viewportHeight; // Top border

                    context.fillRect(startCoordinate, top , Math.max(timelineConfig.minChunkWidth,endCoordinate - startCoordinate), timelineConfig.chunkHeight * scope.viewportHeight);
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
                        context.fillStyle = mouseInScrollbar ? blurColor(timelineConfig.scrollBarHighlightColor, 1) : blurColor(timelineConfig.scrollBarColor, 1);
                        context.fillRect(startCoordinate, top, scrollBarWidth, timelineConfig.scrollBarHeight * scope.viewportHeight);
                    }else{
                        if(mouseInScrollbar || catchScrollBar){
                            mouseInScrollbar = mouseCoordinate - startCoordinate;
                        }
                        if(mouseInScrollbarRow){
                            mouseInScrollbarRow = mouseCoordinate - startCoordinate;
                        }
                        return mouseInScrollbar;
                    }
                }

                // !!! Draw and position for timeMarker
                function drawTimeMarker(context){
                    if(!scope.positionProvider){
                        return;
                    }

                    drawMarker(context, scope.positionProvider.playedPosition, timelineConfig.timeMarkerColor, timelineConfig.timeMarkerTextColor)
                }

                function drawPointerMarker(context){
                    if(!mouseCoordinate || mouseInScrollbar){
                        return;
                    }
                    drawMarker(context, scope.scaleManager.screenCoordinateToDate(mouseCoordinate),timelineConfig.pointerMarkerColor,timelineConfig.pointerMarkerTextColor);
                }

                function drawMarker(context, date,markerColor,textColor){
                    var coordinate =  scope.scaleManager.dateToScreenCoordinate(date);

                    if(coordinate < 0 || coordinate > scope.viewportWidth ) {
                        return;
                    }

                    date = new Date(date);

                    var height = timelineConfig.markerHeight * scope.viewportHeight;

                    // Line
                    context.strokeStyle = blurColor(markerColor,1);
                    context.fillStyle = blurColor(markerColor,1);

                    context.beginPath();
                    context.moveTo(coordinate + 0.5, 0);
                    context.lineTo(coordinate + 0.5, Math.round(scope.viewportHeight - timelineConfig.scrollBarHeight * scope.viewportHeight));
                    context.stroke();

                    var side = null;
                    var startCoord = coordinate - timelineConfig.markerWidth /2;
                    if(startCoord < 0){
                        side = "left";
                        startCoord = 0;
                    }
                    if(startCoord + timelineConfig.markerWidth > scope.viewportWidth){
                        side = "right";
                        startCoord = scope.viewportWidth - timelineConfig.markerWidth;
                    }

                    // Bubble
                    context.fillRect(startCoord, 0, timelineConfig.markerWidth, height );

                    // Triangle
                    context.beginPath();
                    context.moveTo(coordinate + timelineConfig.markerTriangleHeight * scope.viewportHeight, height);
                    context.lineTo(coordinate, height + timelineConfig.markerTriangleHeight * scope.viewportHeight);
                    context.lineTo(coordinate - timelineConfig.markerTriangleHeight * scope.viewportHeight, height);
                    context.closePath();
                    context.fill();

                    // Labels
                    context.fillStyle = blurColor(textColor,1);
                    context.font = formatFont(timelineConfig.markerFont);
                    coordinate = startCoord + timelineConfig.markerWidth /2; // Set actual center of the marker

                    var dateString = dateFormat(date, timelineConfig.dateFormat);
                    var dateWidth = context.measureText(dateString).width;
                    var textStart = (height - timelineConfig.markerFont.size) / 2;
                    context.fillText(dateString,coordinate - dateWidth/2, textStart);

                    dateString = dateFormat(date, timelineConfig.timeFormat);
                    dateWidth = context.measureText(dateString).width;
                    textStart = height/2 + textStart;
                    context.fillText(dateString,coordinate - dateWidth/2, textStart);

                }

                // !!! Mouse events
                var mouseCoordinate = null;
                var mouseRow = 0;
                var mouseInScrollbar = false;
                var mouseInScrollbarRow = false;
                var catchScrollBar = false;

                function updateMouseCoordinate( event){
                    if(!event){
                        mouseRow = 0;
                        mouseCoordinate = null;
                        mouseInScrollbar = false;
                        mouseInScrollbarRow = false;
                        catchScrollBar = false;
                        return;
                    }

                    if($(event.target).is("canvas")){
                        mouseCoordinate = event.offsetX;
                    }else{
                        console.warn ("unexpected node");
                        return; //Something strange - ignore it
                    }
                    mouseRow = event.offsetY;

                    drawOrCheckScrollBar();
                }

                var zoomTarget = 0;
                function mouseWheel(event){
                    updateMouseCoordinate(event);
                    event.preventDefault();
                    if(Math.abs(event.deltaY) > Math.abs(event.deltaX)) { // Zoom or scroll - not both
                        //scope.scaleManager.setAnchorCoordinate( mouseCoordinate );// Set position to keep
                        if(window.jscd.touch ) {

                            zoomTarget = scope.scaleManager.zoom() - event.deltaY / timelineConfig.maxVerticalScrollForZoomWithTouch;

                            scope.scaleManager.zoomAroundDate(
                                zoomTarget,
                                scope.scaleManager.screenCoordinateToDate(mouseCoordinate)
                            );

                        }else{
                            // We need to smooth zoom here

                            if(!animateScope.animating(scope,"zoomCurrent")){ // No animating - flush zoom value. Overwise - collect
                                scope.zoomCurrent = scope.scaleManager.zoom();
                                zoomTarget = scope.zoomCurrent;
                            }
                            // Collect zoom changing in zoomTarget
                            zoomTarget -= event.deltaY / timelineConfig.maxVerticalScrollForZoom;

                            animateScope.animate(scope,"zoomCurrent",zoomTarget).then(
                                function(){},
                                function(){},
                                function(value){
                                    scope.scaleManager.zoomAroundDate(
                                        value,
                                        scope.scaleManager.screenCoordinateToDate(mouseCoordinate)
                                    );
                                }
                            );
                        }
                    } else {
                        scope.scaleManager.scrollByPixels(event.deltaX);
                    }
                    scope.$apply();
                }

                scope.dblClick = function(event){
                    updateMouseCoordinate(event);
                    if(!mouseInScrollbarRow) {
                        scope.scaleManager.setAnchorCoordinate(mouseCoordinate);// Set position to keep
                        scope.zoom(true);
                    }else{
                        if(!mouseInScrollbar){
                            animateScope.animate(scope,"scrollPosition",(mouseInScrollbarRow>0?1:0)).
                                then(
                                    function(){},
                                    function(){},
                                    function(value){
                                        scope.scaleManager.scroll(value);
                                    }
                                );
                        }
                    }
                };
                scope.click = function(event){
                    updateMouseCoordinate(event);
                    if(!mouseInScrollbarRow) {
                        scope.scaleManager.setAnchorCoordinate(mouseCoordinate);// Set position to keep
                        scope.positionHandler(scope.scaleManager.screenCoordinateToDate(mouseCoordinate));
                    }else{
                        if(!mouseInScrollbar){
                            scope.scrollPosition = scope.scaleManager.scroll() ;

                            animateScope.animate(scope,"scrollPosition", mouseCoordinate / scope.viewportWidth).
                                then(
                                    function(){},
                                    function(){},
                                    function(value){
                                        scope.scaleManager.scroll(value);
                                    }
                                );
                        }
                    }
                };


                scope.mouseUp = function(event){
                    updateMouseCoordinate(event);
                    catchScrollBar = false;
                    console.log("set timer for dblclick here");
                    console.log("move playing position");
                };
                scope.mouseLeave = function(event){
                    updateMouseCoordinate(null);
                };
                scope.mouseMove = function(event){
                    updateMouseCoordinate(event);
                    if(catchScrollBar){
                        var moveScroll = mouseInScrollbar - catchScrollBar;
                        scope.scaleManager.scroll( scope.scaleManager.scroll() + moveScroll / scope.viewportWidth );
                    }
                };
                scope.mouseDown = function(event){
                    updateMouseCoordinate(event);
                    catchScrollBar = mouseInScrollbar;
                };

                // !!! Scrolling functions


                // !!! Functions for buttons on view
                scope.zoom = function(zoomIn){
                    scope.zoomTarget = scope.scaleManager.zoom();
                    var zoomTarget = scope.zoomTarget - (zoomIn?1:-1) * timelineConfig.zoomSpeed;
                    animateScope.animate(scope,"zoomTarget",zoomTarget).then(function(){},function(){},
                        function(value){
                            scope.scaleManager.zoom(value);
                        }
                    );

                };

                scope.goToLive = function(){
                    scope.scaleManager.setAnchorCoordinate(1);// Set position to keep
                    scope.positionHandler(false);
                };

                scope.playPause = function(){
                    scope.playHandler(!scope.positionProvider.playing)
                };

                // !!! Subscribe for different events which affect timeline
                $( window ).resize(updateTimelineWidth);    // Adjust width after window was resized
                scope.$watch('recordsProvider',function(){ // RecordsProvider was changed - means new camera was selected
                    if(scope.recordsProvider) {
                        scope.recordsProvider.ready.then(initTimeline);// reinit timeline here
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

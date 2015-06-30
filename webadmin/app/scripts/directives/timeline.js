'use strict';

angular.module('webadminApp')
    .directive('timeline', ['$interval','$timeout','animateScope', function ($interval,$timeout,animateScope) {
        return {
            restrict: 'E',
            scope: {
                recordsProvider: '=',
                positionProvider: '=',

                ngClick: '&',
                handler: '='
            },
            templateUrl: 'views/components/timeline.html',
            link: function (scope, element/*, attrs*/) {
                var timelineConfig = {
                    initialInterval: 1000*60*60*24*365, // no records - show last hour
                    minMsPerPixel: 1,

                    zoomBase: 2, // Parameter for log
                    zoomSpeed: 0.05, // Zoom speed 0 -> 1 = full zoom
                    maxVerticalScrollForZoom: 5000, // value for adjusting zoom
                    horizontalScrollSpeed: 1/10,  // value for adjusting scroll from touchpad
                    animationDuration: 300, // 200-400 for smooth animation

                    timelineBgColor: [28,35,39], //Color for ruler marks and labels
                    font:'sans-serif',

                    topLabelHeight: 20/100, //% // Size for top label text
                    topLabelFontSize: 12, //px // Font size
                    topLabelTextColor: [255,255,255], // Color for text for top labels

                    labelHeight:20/100, // %
                    labelFontSize: 12, // px
                    labelTextColor: [128,128,128],

                    chunkHeight:40/100, // %    //Height for event line
                    minChunkWidth: 1,
                    chunksBgColor:[34,57,37],
                    exactChunkColor: [58,145,30],
                    loadingChunkColor: [58,145,30,0.5],

                    scrollBarHeight: 20/100, // %
                    minScrollBarWidth: 50, // px
                    scrollBarBgColor: [0,0,0],
                    scrollBarColor: [128,128,128],
                    scrollBarHighlightColor: [200,200,200],

                    timeMarkerColor:[128,128,255], // Timemarker color
                    dateFormat:'dd.mm.yyyy', // Timemarker format for date
                    timeFormat:'HH:MM:ss', // Timemarker format for time

                    scrollBoundariesPrecision: 0.000001, // Where we should disable right and left scroll buttons
                    scrollingSpeed: 0.25,// Scrolling speed for scroll buttons right-left


                    end: 0
                };

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
                var timeMarker = element.find(".timeMarker");
                var canvas = element.find('canvas').get(0);
                scope.scaleManager = new ScaleManager(timelineConfig.zoomBase, timelineConfig.minMsPerPixel, timelineConfig.initialInterval, 100); //Init boundariesProvider

                // !!! Initialization functions
                function updateTimelineHeight(){
                    scope.viewportHeight = element.find("canvas").height();
                    canvas.height = scope.viewportHeight;
                }
                function updateTimelineWidth(){
                    scope.viewportWidth = element.find("canvas").width();
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
                    var context = clearTimeline();
                    drawTopLabels(context);
                    drawLowerLabels(context);
                    drawEvents(context);
                    drawScrollBar(context);
                    drawTimeMarker(context);
                }
                function blurColor(color,alpha){ // Bluring function [r,g,b] + alpha -> String
                    /*
                     var colorString =  'rgba(' +
                     Math.round(color[0]) + ',' +
                     Math.round(color[1]) + ',' +
                     Math.round(color[2]) + ',' +
                     alpha + ')';
                     */
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
                    var colorString =  'rgb(' +
                        Math.round(color[0] * alpha + 255 * (1 - alpha)) + ',' +
                        Math.round(color[1] * alpha + 255 * (1 - alpha)) + ',' +
                        Math.round(color[2] * alpha + 255 * (1 - alpha)) + ')';

                    return colorString;
                }
                function clearTimeline(){
                    var context = canvas.getContext('2d');
                    context.fillStyle = blurColor(timelineConfig.timelineBgColor,1);
                    context.fillRect(0, 0, scope.viewportWidth, scope.viewportHeight);
                    return context;
                }

                // !!! Labels logic
                function drawTopLabels(context){
                    var level = scope.scaleManager.levels.topLabels.level;
                    var start = scope.scaleManager.alignStart(level);
                    var end = scope.scaleManager.alignEnd(level);
                    while(start <= end){
                        drawTopLabel(context,start,level);
                        start = level.interval.addToDate(start);
                    }
                }
                function drawTopLabel(context, date, level){
                    var coordinate = scope.scaleManager.dateToScreenCoordinate(date);
                    var label = dateFormat(new Date(date), level.topFormat);

                    context.font = timelineConfig.topLabelFontSize  + 'px ' + timelineConfig.font;
                    context.fillStyle = blurColor(timelineConfig.topLabelTextColor,1);

                    var textWidth = context.measureText(label).width;

                    var textStart = 0 * scope.viewportHeight // Top border
                        + (timelineConfig.topLabelHeight * scope.viewportHeight  - timelineConfig.topLabelFontSize) / 2 // Top padding
                        + timelineConfig.topLabelFontSize; // Font size

                    var x = Math.max(0,coordinate - textWidth/2);

                    context.fillText(label, x, textStart);
                }
                function drawLowerLabels(context){
                    var level = scope.scaleManager.levels.labels.level;
                    var start = scope.scaleManager.alignStart(level);
                    var end = scope.scaleManager.alignEnd(level);

                    while(start <= end){
                        drawLowerLabel(context,start,level);
                        start = level.interval.addToDate(start);
                    }
                }
                function drawLowerLabel(context,date,level){
                    var coordinate = scope.scaleManager.dateToScreenCoordinate(date);
                    var label = dateFormat(new Date(date), level.format);
                    context.font = timelineConfig.labelFontSize  + 'px ' + timelineConfig.font;
                    context.fillStyle = blurColor(timelineConfig.labelTextColor,1);

                    var textWidth = context.measureText(label).width;

                    var textStart = timelineConfig.topLabelHeight * scope.viewportHeight  // Top border
                        + (timelineConfig.labelHeight * scope.viewportHeight  - timelineConfig.labelFontSize) / 2 // Top padding
                        + timelineConfig.labelFontSize; // Font size


                    var x = coordinate - textWidth/2;

                    if(coordinate < 0)
                        return;

                    context.fillText(label, x, textStart);
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
                    context.stroke();
                }

                // !!! Draw ScrollBar
                function drawScrollBar(context){
                    //1. DrawBG
                    context.fillStyle =   blurColor(timelineConfig.scrollBarBgColor,1);

                    var top = (timelineConfig.topLabelHeight + timelineConfig.labelHeight + timelineConfig.chunkHeight) * scope.viewportHeight; // Top border

                    context.fillRect(0, top, scope.viewportWidth , timelineConfig.scrollBarHeight * scope.viewportHeight);

                    //2.
                    var relativeStart =  scope.scaleManager.getRelativeStart();
                    var relativeWidth =  scope.scaleManager.getRelativeWidth();

                    var width = Math.max(scope.viewportWidth * relativeWidth, timelineConfig.minScrollBarWidth);
                    var startCoordinate = (scope.viewportWidth - width) * relativeStart;

                    context.fillStyle =  blurColor(timelineConfig.scrollBarColor,1);
                    context.fillRect(startCoordinate, top, width , timelineConfig.scrollBarHeight * scope.viewportHeight);
                }

                // !!! Draw and position for timeMarker
                function drawTimeMarker(context){
                    var lastCoord = scope.positionCoordinate;
                    if(!scope.positionProvider){
                        timeMarker.addClass("hiddenTimemarker");
                        return;
                    }
                    var date = scope.positionProvider.playedPosition;
                    scope.positionCoordinate =  scope.scaleManager.dateToScreenCoordinate(date);

                    if(scope.positionCoordinate != lastCoord){
                        if(scope.positionCoordinate > 0 && scope.positionCoordinate < scope.viewportWidth) {
                            timeMarker.removeClass("hiddenTimemarker");
                            timeMarker.css("left", scope.positionCoordinate + "px");
                        }else{
                            timeMarker.addClass("hiddenTimemarker");
                        }
                    }

                    date = new Date(scope.positionProvider.playedPosition);
                    scope.playingDate = dateFormat(date, timelineConfig.dateFormat);
                    scope.playingTime = dateFormat(date, timelineConfig.timeFormat);

                    context.strokeStyle = blurColor(timelineConfig.timeMarkerColor,1);
                    context.fillStyle = blurColor(timelineConfig.timeMarkerColor,1);

                    context.beginPath();
                    context.moveTo(scope.positionCoordinate, 0);
                    context.lineTo(scope.positionCoordinate, canvas.height);
                    context.stroke();
                }



                // !!! Mouse events
                var mouseDate = null;
                var mouseCoordinate = null;


                function updateMouseCoordinate( event){
                    if(!event){
                        mouseCoordinate = null;
                        mouseDate = null;
                        return;
                    }
                    mouseCoordinate = event.offsetX;
                    mouseDate = scope.scaleManager.screenCoordinateToDate(mouseCoordinate);
                }

                function mouseWheel(event){
                    updateMouseCoordinate(event);
                    event.preventDefault();
                    console.log("mouseWheel",event.deltaY,event.deltaX);
                    if(Math.abs(event.deltaY) > Math.abs(event.deltaX)) { // Zoom or scroll - not both
                        scope.scaleManager.setAnchorDateAndPoint(mouseDate, mouseCoordinate/scope.viewportWidth);// Set position to keep
                        scope.scaleManager.zoomIn(event.deltaY / timelineConfig.maxVerticalScrollForZoom);
                    } else {
                        scope.scaleManager.scrollByPixels(event.deltaX * timelineConfig.horizontalScrollSpeed);
                    }
                    scope.$apply();
                }

                scope.dblClick = function(event){
                    updateMouseCoordinate(event);
                    event.preventDefault();
                    scope.scaleManager.setAnchorDateAndPoint(mouseDate, mouseCoordinate/scope.viewportWidth);// Set position to keep
                    scope.zoom(true);
                };
                scope.click = function(event){
                    updateMouseCoordinate(event);
                    scope.scaleManager.setAnchorDateAndPoint(mouseDate, mouseCoordinate/scope.viewportWidth);// Set position to keep
                    scope.handler(mouseDate);
                };


                scope.mouseUp = function(event){
                    updateMouseCoordinate(event);
                    console.log("set timer for dblclick here");
                    console.log("move playing position");
                };
                scope.mouseLeave = function(event){
                    updateMouseCoordinate(null);
                    console.log("mouse leave - stop drag and drop");
                };
                scope.mouseMove = function(event){
                    updateMouseCoordinate(event);
                };

                scope.mouseDown = function(event){
                    updateMouseCoordinate(event);
                };

                // !!! Scrolling functions


                // !!! Functions for buttons on view
                scope.zoom = function(zoomIn){
                    scope.scaleManager.zoomIn((zoomIn?1:-1) * timelineConfig.zoomSpeed);
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

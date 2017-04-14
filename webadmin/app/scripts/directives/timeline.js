'use strict';

angular.module('webadminApp')
    .directive('timeline', ['$interval', '$timeout', 'animateScope', '$q', function ($interval, $timeout, animateScope, $q) {
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
                 *
                 * timelineRender (timelineCanvasRender) - GUI module which draws timeline and detects mouse position over elements
                 */

                var debugEventsMode = Config.debug.chunksOnTimeline && Config.allowDebugMode;
                scope.playbackNotSupported = window.jscd.mobile;

                // !!! Read basic parameters, DOM elements and global objects for module
                var viewport = element.find('.viewport');
                var $canvas = element.find('canvas');
                var canvas = $canvas.get(0);


                var timelineConfig = TimelineConfig();

                scope.scaleManager = new ScaleManager( timelineConfig.minMsPerPixel,
                    timelineConfig.maxMsPerPixel,
                    timelineConfig.initialInterval,
                    100,
                    timelineConfig.stickToLiveMs,
                    timelineConfig.zoomAccuracyMs,
                    timelineConfig.lastMinuteDuration,
                    timelineConfig.minPixelsPerLevel,
                    $q); //Init boundariesProvider

                var animationState = {
                    targetLevels : scope.scaleManager.levels,
                    currentLevels: {events:{index:0},labels:{index:0},middle:{index:0},small:{index:0},marks:{index:0}},
                    zooming:1
                };

                var timelineRender = new TimelineCanvasRender(canvas,
                    timelineConfig,
                    scope.recordsProvider,
                    scope.scaleManager,
                    animationState,
                    {
                        debugEventsMode:debugEventsMode,
                        allowDebug:Config.allowDebugMode
                    });

                var timelineActions = new TimelineActions(
                    timelineConfig,
                    scope.positionProvider,
                    scope.scaleManager,
                    animationState,
                    animateScope,
                    scope);

                // !!! Initialization functions
                function updateTimelineHeight(){
                    canvas.height = element.find('.viewport').height();
                }
                function updateTimelineWidth(){
                    scope.viewportWidth = element.find('.viewport').width();
                    canvas.width  = scope.viewportWidth;
                    scope.scaleManager.setViewportWidth(scope.viewportWidth);
                    $timeout(function(){
                        scope.scaleManager.checkZoom();
                    });
                }
                function initTimeline(){
                    var now = (new Date()).getTime();
                    scope.scaleManager.setStart(scope.recordsProvider && scope.recordsProvider.chunksTree ? scope.recordsProvider.chunksTree.start : (now - timelineConfig.initialInterval));
                    scope.scaleManager.setEnd(now);

                    fullZoomOut(); // Animate full zoom out
                }

                // !!! Render everything: updating function
                function render(){

                    timelineActions.updatePosition();

                    if(scope.recordsProvider) {
                        scope.recordsProvider.updateLastMinute(timelineConfig.lastMinuteDuration, scope.scaleManager.levels.events.index);
                    }

                    zoomingRenew();
                    timelineActions.scrollingRenew();

                    timelineRender.Draw( mouseXOverTimeline, mouseYOverTimeline, catchScrollBar);
                }


                /**
                 * Events model for timeline:
                 *
                 * Slider:
                 *      + Drag - Scroll
                 *
                 * Scrollbar:
                 *      + Click - Scroll to one screen
                 *      + DblClick - Scroll to clicked point
                 *      + Hold -  Constant Scrolling
                 *
                 * Scrollbar buttons (left/right):
                 *      + Click - Scroll to part of the screen
                 *      + DblClick - Scroll to the end
                 *      + Hold -  Constant Scrolling
                 *
                 * Other timeline parts:
                 *      Over - Show timemarker, no actions
                 *      + Click - Playing position
                 *      + DblClick - Playing position and zoom in
                 *      + Drag - Scroll
                 *      + MouseWheel - right-left - Scroll
                 *      + MouseWheel - up-down - Zoom
                 *
                 * Zoom buttons:
                 *      + Click - On step zoom
                 *      + DblClick on zoomout - full zoom out
                 *      + Hold - Constant zooming
                 */

                function goToLive(force){
                    timelineActions.goToLive(force);
                    if(!scope.positionProvider.liveMode || force) {
                        scope.positionHandler(false);
                    }
                }

                function playPause(){
                    timelineActions.playPause();
                    scope.playHandler(!scope.positionProvider.playing);
                }

                function timelineClick(mouseX){
                    var date = timelineActions.setAnchorCoordinate(mouseX);

                    var lastMinute = scope.scaleManager.lastMinute();
                    if(date > lastMinute){
                        goToLive ();
                    }else {
                        scope.positionHandler(date);
                    }
                }



                /**
                 * Scrolling functions
                 */



                // High-level Handlers
                function scrollbarClickOrHold(left){
                    timelineActions.scrollingStart(left, timelineConfig.scrollSpeed * scope.viewportWidth);
                }
                function scrollbarDblClick(mouseX){
                    timelineActions.animateScroll(mouseX / scope.viewportWidth);
                }

                function scrollButtonClickOrHold(left){
                    timelineActions.scrollingStart(left, timelineConfig.scrollButtonSpeed * scope.viewportWidth);
                }
                function scrollButtonDblClick(left){
                    timelineActions.animateScroll(left ? 0 : 1);
                }

                function scrollByWheel(pixels){
                    timelineActions.scrollByPixels(pixels);
                }

                var catchScrollBar = false;
                function scrollbarSliderDragStart(mouseX){
                    scope.scaleManager.stopWatching();
                    catchScrollBar = mouseX;
                }
                function scrollbarSliderDrag(mouseX){
                    if(catchScrollBar) {
                        var moveScroll = mouseX - catchScrollBar;
                        scope.scaleManager.scroll(scope.scaleManager.scroll() + moveScroll / scope.viewportWidth);
                        catchScrollBar = mouseX;
                        return moveScroll !== 0;
                    }
                    return false;
                }
                function scrollbarSliderDragEnd(){
                    if(catchScrollBar) {
                        scope.scaleManager.releaseWatching();
                        catchScrollBar = false;
                    }
                }

                var catchTimeline = false;
                function timelineDragStart(mouseX){
                    scope.scaleManager.stopWatching();
                    catchTimeline = mouseX;
                }
                function timelineDrag(mouseX){
                    if(catchTimeline) {
                        var moveScroll = catchTimeline - mouseX;
                        scope.scaleManager.scrollByPixels(moveScroll);
                        catchTimeline = mouseX;
                        return (moveScroll !== 0 );
                    }
                }
                function timelineDragEnd(){
                    if(catchTimeline) {
                        scope.scaleManager.releaseWatching();
                        catchTimeline = false;
                    }
                }

                /**
                 * Zooming functions
                 */



                //Relative zoom (step)
                function zoom(zoomIn,slow,linear, zoomDate){
                    var zoomTarget = scope.scaleManager.zoom() - (zoomIn ? 1 : -1) * (slow?timelineConfig.slowZoomSpeed:timelineConfig.zoomSpeed);
                    timelineActions.zoomTo(zoomTarget, zoomDate, false, linear);
                }

                var zoomingNow = false;
                var zoomingIn = false;
                function zoomingRenew(){ // renew zooming
                    if(zoomingNow) {
                        zoom(zoomingIn, true, false);
                    }
                }
                function zoomingStop() {
                    if(zoomingNow) {
                        zoomingNow = false;
                        zoom(zoomingIn, true, true);
                    }
                }
                function zoomingStart(zoomIn) {
                    if(scope.scaleManager.disableZoomOut&&!zoomIn || scope.scaleManager.disableZoomIn&&zoomIn){
                        return;
                    }

                    zoomingNow = true;
                    zoomingIn = zoomIn;

                    zoomingRenew();
                }


                function fullZoomOut(){
                    timelineActions.zoomTo(1);
                }

                function timelineDblClick(mouseX){
                    zoom(true,false,false, scope.scaleManager.setAnchorCoordinate(mouseX));
                }

                function zoomInClickOrHold(){
                    zoomingStart(true);
                }

                function zoomOutClickOrHold(){
                    zoomingStart(false);
                }

                function zoomOutDblClick(){
                    zoomingStop();
                    fullZoomOut();
                }


                var zoomByWheelTarget = 0;
                function zoomByWheel(pixels){

                    var zoom = scope.scaleManager.zoom();

                    if(window.jscd.touch ) {
                        zoomByWheelTarget = zoom - pixels / timelineConfig.maxVerticalScrollForZoomWithTouch;
                    }else{
                        // We need to smooth zoom here
                        // Collect zoom changing in zoomTarget
                        if(!zoomByWheelTarget) {
                            zoomByWheelTarget = zoom;
                        }
                        zoomByWheelTarget -= pixels / timelineConfig.maxVerticalScrollForZoom;
                        zoomByWheelTarget = scope.scaleManager.boundZoom(zoomByWheelTarget);
                    }

                    var zoomDate = scope.scaleManager.screenCoordinateToDate(mouseXOverTimeline);
                    if(mouseOverElements.rightBorder && !mouseOverElements.rightButton){
                        zoomDate = scope.scaleManager.end;
                    }
                    if(mouseOverElements.leftBorder && !mouseOverElements.leftButton){
                        zoomDate = scope.scaleManager.start;
                    }

                    timelineActions.zoomTo(zoomByWheelTarget, zoomDate, window.jscd.touch);
                }


                /**
                 * Actual browser events handling
                 */


                // !!! Mouse events
                var mouseXOverTimeline = 0;
                var mouseYOverTimeline = 0;
                var mouseOverElements = null;

                function updateMouseCoordinate(event){
                    if(!event){
                        mouseYOverTimeline = 0;
                        mouseXOverTimeline = 0;
                        mouseOverElements = null;
                        return;
                    }

                    if($(event.target).is('canvas') && event.offsetX){
                        mouseXOverTimeline = event.offsetX;
                    }else{
                        mouseXOverTimeline = event.pageX - $(canvas).offset().left;
                    }
                    mouseYOverTimeline = event.offsetY || (event.pageY - $(canvas).offset().top);

                    mouseOverElements = timelineRender.checkElementsUnderCursor(mouseXOverTimeline,mouseYOverTimeline);

                }

                function viewportMouseWheel(event){
                    updateMouseCoordinate(event);
                    event.preventDefault();
                    var vertical = Math.abs(event.deltaY) > Math.abs(event.deltaX);

                    if(vertical) { // Zoom or scroll - not both
                        zoomByWheel(event.deltaY);
                    } else {
                        scrollByWheel(event.deltaX);
                    }
                    scope.$apply();
                }


                function canvasDragInit(event){
                    updateMouseCoordinate(event);
                }

                var dragged = false;
                function canvasDragStart(event){
                    updateMouseCoordinate(event);
                    dragged = false;
                    /*if(mouseOverElements.leftButton){
                     scope.startScroll(true);
                     return;
                     }

                     if(mouseOverElements.rightButton){
                     scope.startScroll(false);
                     return;
                     }*/

                    if(mouseOverElements.scrollbarSlider){
                        scrollbarSliderDragStart(mouseXOverTimeline);
                    }
                    if(mouseOverElements.timeline){
                        timelineDragStart(mouseXOverTimeline)
                    }
                }
                function canvasDrag(event){
                    updateMouseCoordinate(event);
                    dragged = scrollbarSliderDrag(mouseXOverTimeline) ||
                        timelineDrag (mouseXOverTimeline);
                }

                var preventClick = false;
                function canvasDragEnd(){
                    updateMouseCoordinate(null);
                    scrollbarSliderDragEnd();
                    timelineDragEnd();

                    if(dragged){
                        preventClick = true;
                        setTimeout(function(){
                            preventClick = false;
                        }, timelineConfig.animationDuration);
                    }
                }


                function viewportDblClick(event){
                    updateMouseCoordinate(event);

                    if(mouseOverElements.leftButton){
                        scrollButtonDblClick(true);
                        return;
                    }

                    if(mouseOverElements.rightButton){
                        scrollButtonDblClick(false);
                        return;
                    }
                    if(mouseOverElements.timeline){
                        timelineDblClick(mouseXOverTimeline);
                        return;
                    }

                    if(mouseOverElements.scrollbar && !mouseOverElements.scrollbarSlider){
                        scrollbarDblClick(mouseXOverTimeline);
                    }
                }
                function viewportMouseDown(event){
                    updateMouseCoordinate(event);

                    if(mouseOverElements.leftButton){
                        scrollButtonClickOrHold(true);
                        return;
                    }

                    if(mouseOverElements.rightButton){
                        scrollButtonClickOrHold(false);
                        return;
                    }

                    if(mouseOverElements.scrollbar && !mouseOverElements.scrollbarSlider){
                        scrollbarClickOrHold(mouseXOverTimeline);
                    }
                }
                function viewportClick(event){
                    updateMouseCoordinate(event);

                    if(preventClick){
                        return;
                    }
                    if(mouseOverElements.timeline){
                        timelineClick(mouseXOverTimeline);
                    }
                }

                function viewportMouseUp(){
                    updateMouseCoordinate(null);
                    timelineActions.scrollingStop(false);
                }
                function viewportMouseMove(event){
                    updateMouseCoordinate(event);
                }
                function viewportMouseLeave(event){
                    updateMouseCoordinate(event);
                }

                /**
                 * Bind events to html
                 */

                // 0. Angular handlers for GUI
                scope.goToLive = goToLive;
                scope.playPause = playPause;

                // 1. Visual buttons

                element.on('mouseup',      '.zoomOutButton', zoomingStop);
                element.on('mouseleave',   '.zoomOutButton', zoomingStop);
                element.on('mousedown',    '.zoomOutButton', zoomOutClickOrHold);
                element.on('dblclick',     '.zoomOutButton', zoomOutDblClick);

                element.on('mouseup',      '.zoomInButton',  zoomingStop);
                element.on('mouseleave',   '.zoomInButton',  zoomingStop);
                element.on('mousedown',    '.zoomInButton',  zoomInClickOrHold);

                //2. Canvas events
                $( window ).resize(updateTimelineWidth);    // Adjust width after window was resized
                $canvas.bind('contextmenu', function(e) { e.preventDefault(); }); //Disable context menu on timeline

                viewport.mouseup(viewportMouseUp);
                viewport.mousemove(viewportMouseMove);
                viewport.mouseleave(viewportMouseLeave);
                // viewport.mousedown(viewportMouseDown);
                viewport.dblclick(viewportDblClick);
                viewport.click(viewportClick);
                viewport.mousewheel(viewportMouseWheel);


                $canvas.drag('draginit',viewportMouseDown); // draginit overrides mousedown
                $canvas.drag('dragstart',canvasDragStart);
                $canvas.drag('drag',canvasDrag);
                $canvas.drag('dragend',canvasDragEnd);

                $canvas.bind('touchstart', canvasDragStart);
                $canvas.bind('touchmove', canvasDrag);
                $canvas.bind('touchend', canvasDragEnd);
                //$canvas.bind('touchcancel', canvasDragEnd);
                // Actual browser events handling





                scope.$watch('positionProvider',function(){
                    timelineActions.setPositionProvider(scope.positionProvider);
                });

                // !!! Subscribe for different events which affect timeline
                scope.$watch('positionProvider.liveMode',function(){
                    if(scope.positionProvider && scope.positionProvider.liveMode) {
                        goToLive (true);
                    }
                });

                if(scope.positionProvider) {
                    scope.playingTime = dateFormat(scope.positionProvider.playedPosition,timelineConfig.dateFormat + ' ' + timelineConfig.timeFormat);
                }
                if(scope.playbackNotSupported) {
                    scope.$watch('positionProvider.playedPosition', function () {
                        if(scope.positionProvider) {
                            scope.playingTime = dateFormat(scope.positionProvider.playedPosition, timelineConfig.dateFormat + ' ' + timelineConfig.timeFormat);
                        }
                    });
                }

                scope.$watch('recordsProvider',function(){ // RecordsProvider was changed - means new camera was selected
                    if(scope.recordsProvider) {
                        scope.recordsProvider.ready.then(initTimeline);// reinit timeline here
                        scope.recordsProvider.archiveReadyPromise.then(function(hasArchive){
                            scope.emptyArchive = !hasArchive;
                        });

                        timelineRender.setRecordsProvider(scope.recordsProvider);
                    }
                });

                // !!! Finally run required functions to initialize timeline
                updateTimelineHeight();
                updateTimelineWidth(); // Adjust width
                initTimeline(); // Setup boundaries and scale

                // !!! Start drawing
                animateScope.setDuration(timelineConfig.animationDuration);
                animateScope.setScope(scope);
                animateScope.start(render);
                scope.$on('$destroy', function() { animateScope.stop(); });
            }
        };
    }]);

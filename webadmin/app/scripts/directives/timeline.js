'use strict';

angular.module('webadminApp')
    .directive('timeline', ['$interval','$timeout','animateScope', function ($interval,$timeout,animateScope) {
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
                    timelineConfig.zoomAccuracy,
                    timelineConfig.lastMinuteDuration); //Init boundariesProvider

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

                // !!! Initialization functions
                function updateTimelineHeight(){
                    canvas.height = element.find('.viewport').height();
                }
                function updateTimelineWidth(){
                    scope.viewportWidth = element.find('.viewport').width();
                    canvas.width  = scope.viewportWidth;
                    scope.scaleManager.setViewportWidth(scope.viewportWidth);
                    $timeout(checkZoomButtons);
                }
                function initTimeline(){
                    var now = (new Date()).getTime();
                    scope.scaleManager.setStart(scope.recordsProvider && scope.recordsProvider.chunksTree ? scope.recordsProvider.chunksTree.start : (now - timelineConfig.initialInterval));
                    scope.scaleManager.setEnd(now);

                    fullZoomOut(); // Animate full zoom out
                }

                // !!! Render everything: updating function
                var lastPlayedPosition = 0;
                function render(){
                    if(scope.positionProvider) {
                        var duration = timelineConfig.animationDuration;
                        if(Math.abs(lastPlayedPosition - scope.positionProvider.playedPosition) > 2 * duration ){
                            if(!animateScope.animating(scope, 'jumpingPosition')) {
                                animateScope.progress(scope, 'jumpingPosition', "smooth", duration).then(
                                    function () {
                                        lastPlayedPosition = scope.positionProvider.playedPosition;
                                    },
                                    function () {
                                    },
                                    function (val) {
                                        var activeDate = lastPlayedPosition + val * (scope.positionProvider.playedPosition - lastPlayedPosition);
                                        scope.scaleManager.tryToSetLiveDate(activeDate, scope.positionProvider.liveMode, (new Date()).getTime());
                                    });
                            }
                        }else{
                            scope.scaleManager.tryToSetLiveDate(scope.positionProvider.playedPosition,scope.positionProvider.liveMode,(new Date()).getTime());
                            lastPlayedPosition = scope.positionProvider.playedPosition;
                        }



                    }
                    if(scope.recordsProvider) {
                        scope.recordsProvider.updateLastMinute(timelineConfig.lastMinuteDuration, scope.scaleManager.levels.events.index);
                    }

                    zoomingRenew();
                    scrollingRenew();

                    timelineRender.Draw( mouseXOverTimeline, mouseYOverTimeline, catchScrollBar);
                }


                /**
                 *
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
                    var moveDate = scope.scaleManager.screenCoordinateToDate(1);
                    animateScope.progress(scope, 'goingToLive' ).then(
                        function(){
                            var activeDate = (new Date()).getTime();
                            scope.scaleManager.setAnchorDateAndPoint(activeDate,1);
                            scope.scaleManager.watchPlaying(activeDate, true);
                        },
                        function(){},
                        function(val){
                            var activeDate = moveDate + val * ((new Date()).getTime() - moveDate);
                            scope.scaleManager.setAnchorDateAndPoint(activeDate,1);
                        });

                    if(!scope.positionProvider.liveMode || force) {
                        scope.positionHandler(false);
                    }
                }

                function playPause(){
                    if(!scope.positionProvider.playing){
                        scope.scaleManager.watchPlaying();
                    }

                    scope.playHandler(!scope.positionProvider.playing);
                }

                function timelineClick(mouseX){
                    scope.scaleManager.setAnchorCoordinate(mouseX);// Set position to keep
                    var date = scope.scaleManager.screenCoordinateToDate(mouseX);
                    lastPlayedPosition = date;
                    var lastMinute = scope.scaleManager.lastMinute();
                    if(date > lastMinute){
                        goToLive ();
                    }else {
                        scope.positionHandler(date);
                        scope.scaleManager.watchPlaying(date);
                    }
                }

                // Disable watching for playing position during any animations
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


                /**
                 * Scrolling functions
                 */

                // linear is for holding function
                function animateScroll(targetPosition,linear){
                    delayWatchingPlayingPosition();
                    scope.scrollTarget = scope.scaleManager.scroll();
                    animateScope.animate(scope, 'scrollTarget', targetPosition, linear?'linear':false).
                        then(
                        function(){
                            scrollingNow = false;
                        },
                        function(){},
                        function(value){
                            scope.scaleManager.scroll(value);
                        }
                    );
                }

                // Constant scrolling process
                var scrollingNow = false;
                var scrollingLeft = false;
                var scrollingSpeed = 0;
                function scrollingRenew(stoping){
                    if(scrollingNow) {
                        var moveScroll = (scrollingLeft ? -1 : 1) * scrollingSpeed * scope.viewportWidth;
                        var scrollTarget = scope.scaleManager.getScrollByPixelsTarget(moveScroll);
                        animateScroll(scrollTarget,stoping);
                    }
                }
                function scrollingStart(left,speed){
                    scrollingLeft = left;
                    scrollingNow = true;
                    scrollingSpeed = speed;
                    scrollingRenew ();
                }
                function scrollingStop(){
                    if(scrollingNow) {
                        scrollingRenew(true); // Smooth slowdown for scrolling
                        scrollingNow = false;
                    }
                }

                // High-level Handlers
                function scrollbarClickOrHold(left){
                    scrollingStart(left,timelineConfig.scrollSpeed);
                }
                function scrollbarDblClick(mouseX){
                    animateScroll(mouseX / scope.viewportWidth);
                }

                function scrollButtonClickOrHold(left){
                    scrollingStart(left,timelineConfig.scrollButtonSpeed);
                }
                function scrollButtonDblClick(left){
                    animateScroll(left ? 0 : 1);
                }

                function scrollByWheel(pixels){
                    scope.scaleManager.scrollByPixels(pixels);
                    delayWatchingPlayingPosition();
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

                //Absolute zoom - to target level from 0 to 1
                function zoomTo(zoomTarget, zoomDate, instant, linear){
                    zoomTarget = scope.scaleManager.boundZoom(zoomTarget);


                    function levelsChanged(newLevels,oldLevels){
                        if(newLevels && (!oldLevels || !oldLevels.labels)){
                            return true;
                        }

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



                    //Find final levels for this zoom and run animation:
                    var newTargetLevels = scope.scaleManager.targetLevels(zoomTarget);
                    if(levelsChanged(newTargetLevels, animationState.targetLevels)){
                        animationState.targetLevels = newTargetLevels;

                        if( animationState.zooming == 1){ // We need to run animation again
                            animationState.zooming = 0;
                        }

                        // This allows us to continue (and slowdown, mb) animation every time
                        scope.zooming = animationState.zooming;
                        animateScope.animate(scope,'zooming',1,'dryResistance').then(function(){
                            animationState.currentLevels = scope.scaleManager.levels;
                        },function(){
                            // ignore animation re-run
                        },function(value){
                            animationState.zooming = value;
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
                        animateScope.animate(scope, 'zoomTarget', zoomTarget, linear?'linear':'dryResistance').then(
                            function () {},
                            function () {},
                            setZoom);
                    }else{
                        setZoom(zoomTarget);
                        scope.zoomTarget = scope.scaleManager.zoom();
                    }
                }

                //Relative zoom (step)
                function zoom(zoomIn,slow,linear, zoomDate){
                    var zoomTarget = scope.scaleManager.zoom() - (zoomIn ? 1 : -1) * (slow?timelineConfig.slowZoomSpeed:timelineConfig.zoomSpeed);
                    zoomTo(zoomTarget, zoomDate, false, linear);
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
                    if(scope.disableZoomOut&&!zoomIn || scope.disableZoomIn&&zoomIn){
                        return;
                    }

                    zoomingNow = true;
                    zoomingIn = zoomIn;

                    zoomingRenew();
                }

                function checkZoomButtons(){
                    scope.disableZoomOut = !scope.scaleManager.çheckZoomOut();
                    scope.disableZoomIn =  !scope.scaleManager.çheckZoomIn();
                }

                function fullZoomOut(){
                    zoomTo(1);
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
                    if(window.jscd.touch ) {
                        zoomByWheelTarget = scope.scaleManager.zoom() - pixels / timelineConfig.maxVerticalScrollForZoomWithTouch;
                    }else{
                        // We need to smooth zoom here
                        // Collect zoom changing in zoomTarget
                        if(!zoomByWheelTarget) {
                            zoomByWheelTarget = scope.scaleManager.zoom();
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

                    zoomTo(zoomByWheelTarget, zoomDate, window.jscd.touch);
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
                    scrollingStop(false);
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

'use strict';

angular.module('webadminApp')
    .directive('timeline', ['$interval', '$timeout', 'animateScope', '$q', function ($interval, $timeout, animateScope, $q) {
        return {
            restrict: 'E',
            scope: {
                canViewArchive: "=",
                recordsProvider: '=',
                positionProvider: '=',
                playHandler: '=',
                liveOnly: '=',
                canPlayLive: '=',
                ngClick: '&',
                positionHandler: '=',
                volumeLevel: '=',
                serverTime: '='
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
                scope.playbackNotSupported = false;//window.jscd.mobile;

                scope.disableVolume = Config.settingsConfig.disableVolume;

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
                    Config.settingsConfig.useServerTime,
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

                    timelineActions.fullZoomOut(); // Animate full zoom out
                }

                // !!! Render everything: updating function
                function render(){
                    if(scope.recordsProvider) {
                        scope.recordsProvider.updateLastMinute(timelineConfig.lastMinuteDuration, scope.scaleManager.levels.events.index);
                    }

                    timelineActions.updateState();
                    timelineRender.Draw( mouseXOverTimeline, mouseYOverTimeline, timelineActions.scrollingNow, timelineActions.catchScrollBar);
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
                    timelineActions.goToLive();
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
                        timelineActions.zoomByWheel(event.deltaY, mouseOverElements, mouseXOverTimeline);
                    } else {
                        timelineActions.scrollByPixels(event.deltaX);
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
                        timelineActions.scrollbarSliderDragStart(mouseXOverTimeline);
                    }
                    if(mouseOverElements.timeline){
                        timelineActions.timelineDragStart(mouseXOverTimeline)
                    }
                }
                function canvasDrag(event){
                    updateMouseCoordinate(event);
                    dragged = timelineActions.scrollbarSliderDrag(mouseXOverTimeline) ||
                              timelineActions.timelineDrag (mouseXOverTimeline);
                }

                var preventClick = false;
                function canvasDragEnd(){
                    updateMouseCoordinate(null);
                    timelineActions.scrollbarSliderDragEnd();
                    timelineActions.timelineDragEnd();

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
                        timelineActions.zoomInToPoint(mouseX);
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
                        var scrollLeft = true;
                        //checking if mouse is to the left or right of the scrollbar
                        if(scope.scaleManager.getRelativeCenter() * canvas.width < mouseXOverTimeline)
                            scrollLeft = false;
                        scrollbarClickOrHold(scrollLeft);
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

                element.on('mouseup',      '.zoomOutButton', function(){
                    timelineActions.zoomingStop();
                });
                element.on('mouseleave',   '.zoomOutButton', function(){
                    timelineActions.zoomingStop();
                });
                element.on('mousedown',    '.zoomOutButton', function(){
                    timelineActions.zoomingStart(false);
                });
                element.on('dblclick',     '.zoomOutButton', function(){
                    timelineActions.fullZoomOut();
                });

                element.on('mouseup',      '.zoomInButton', function(){
                    timelineActions.zoomingStop();
                });
                element.on('mouseleave',   '.zoomInButton', function(){
                    timelineActions.zoomingStop();
                });
                element.on('mousedown',    '.zoomInButton',  function(){
                    timelineActions.zoomingStart(true);
                });

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

                scope.$watch('serverTime.timeZoneOffset', function(){
                    scope.scaleManager.updateServerOffset(scope.serverTime);
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
                scope.$on('$destroy', function() { animateScope.stopScope(scope); });
            }
        };
    }]);

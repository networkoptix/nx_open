'use strict';

angular.module('nxCommon')
    .directive('timeline', ['$interval', '$timeout', 'animateScope', '$q', function ($interval, $timeout, animateScope, $q) {
        return {
            restrict: 'E',
            scope: {
                canViewArchive: "=",
                recordsProvider: '=',
                positionProvider: '=',
                playHandler: '=',
                canPlayLive: '=',
                ngClick: '&',
                positionHandler: '=',
                volumeLevel: '='
            },
            templateUrl: Config.viewsDirCommon + 'components/timeline.html',
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

                scope.disableVolume = Config.webclient.disableVolume;

                // !!! Read basic parameters, DOM elements and global objects for module
                var viewport = element.find('.viewport');
                var $canvas = element.find('canvas');
                var canvas = $canvas.get(0);

                scope.toggleActive = function(){
                    scope.activeVolume = !scope.activeVolume;
                };
                var pixelAspectRatio = function(){
                    if(window.devicePixelRatio > 1)
                    {
                        return window.devicePixelRatio;
                    }

                    var mediaQuery = "(-webkit-min-device-pixel-ratio: 1.5),\
                                      (min--moz-device-pixel-ratio: 1.5),\
                                      (-o-min-device-pixel-ratio: 3/2),\
                                      (min-resolution: 1.5dppx)";
                    if( window.matchMedia && window.matchMedia(mediaQuery).matches){
                        return 2;
                    }
                    return 1;
                }();

                var timelineConfig = TimelineConfig;

                scope.scaleManager = new ScaleManager( timelineConfig.minMsPerPixel,
                    timelineConfig.maxMsPerPixel,
                    timelineConfig.initialInterval,
                    100,
                    timelineConfig.stickToLiveMs,
                    timelineConfig.zoomAccuracyMs,
                    timelineConfig.lastMinuteDuration,
                    timelineConfig.minPixelsPerLevel,
                    timelineConfig.minScrollBarWidth,
                    pixelAspectRatio,
                    $q); //Init boundariesProvider

                var animationState = {
                    targetLevels : scope.scaleManager.levels,
                    currentLevels: {events:{index:0},labels:{index:0},middle:{index:0},small:{index:0},marks:{index:0}},
                    labels:1,
                    middle:1,
                    small:1,
                    marks:1
                };

                var timelineRender = new TimelineCanvasRender(canvas,
                    timelineConfig,
                    scope.recordsProvider,
                    scope.scaleManager,
                    animationState,
                    {
                        debugEventsMode:debugEventsMode,
                        allowDebug:Config.allowDebugMode
                    },
                    pixelAspectRatio);

                var timelineActions = new TimelineActions(
                    timelineConfig,
                    scope.positionProvider,
                    scope.scaleManager,
                    animationState,
                    animateScope,
                    scope);

                // !!! Initialization functions
                function updateTimelineHeight(){
                    canvas.height = element.find('.viewport').height() * pixelAspectRatio;
                }
                function updateTimelineWidth(){
                    scope.viewportWidth = element.find('.viewport').width();
                    canvas.width  = scope.viewportWidth * pixelAspectRatio;
                    scope.scaleManager.setViewportWidth(scope.viewportWidth);
                    $timeout(function(){
                        scope.scaleManager.checkZoom(scope.scaleManager.zoom());
                    });
                }
                function initTimeline(){
                    var now = timeManager.nowToDisplay();
                    scope.scaleManager.setStart(scope.recordsProvider && scope.recordsProvider.chunksTree ? scope.recordsProvider.chunksTree.start : (now - timelineConfig.initialInterval));
                    scope.scaleManager.setEnd();
                    timelineActions.updateZoomLevels(1); // Force update zoom levels
                    timelineActions.fullZoomOut(); // Animate full zoom out
                }

                // !!! Render everything: updating function
                function render(){
                    if(scope.recordsProvider) {
                        scope.recordsProvider.updateLastMinute(timelineConfig.lastMinuteDuration,
                                                               scope.scaleManager.levels.events.index);
                    }

                    timelineActions.updateState(mouseXOverTimeline);
                    timelineRender.Draw(mouseXOverTimeline,
                                        mouseYOverTimeline,
                                        timelineActions.scrollingNow,
                                        timelineActions.catchScrollBar);
                }


                /**
                 * Events model for timeline:
                 *
                 * Slider:
                 *      + Drag - Scroll
                 *
                 * Scrollbar:
                 *      + Click - Scroll to one screen
                 *      + DblClick - Scroll two screens
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
                    if(!scope.positionProvider.playing){
                        scope.playHandler(true);
                        $timeout(function(){
                            goToLive(force);
                        });
                        return;
                    }
                    timelineActions.goToLive();
                    if(!scope.positionProvider.liveMode || force) {
                        scope.positionHandler(false);
                    }
                }

                var pausedLive = false;
                function playPause(){
                    if(scope.positionProvider.playing){
                        pausedLive = scope.positionProvider.liveMode;
                        scope.playHandler(false);
                    }else if(pausedLive){
                        jumpToPosition(scope.positionProvider.playedPosition);
                        scope.playHandler(true);
                    }else{
                        scope.playHandler(true);
                    }

                    timelineActions.playPause();
                }

                function timelineClick(mouseX){
                    var date = timelineActions.setClickedCoordinate(mouseX);
                    jumpToPosition(date);
                }

                function jumpToPosition(date){
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
                function scrollbarClickOrHold(){
                    timelineActions.scrollingToCursorStart().then(function(result){
                        if(result){
                            scrollbarSliderDragStart();
                        }
                    });
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
                    if(event.touches){
                        event.pageX = event.touches[0].pageX;
                        event.pageY = event.touches[0].pageY;
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
                    var vertical = Math.abs(event.deltaY) >= Math.abs(event.deltaX);

                    if(vertical) { // Zoom or scroll - not both
                        timelineActions.zoomByWheel(event.deltaY, mouseOverElements, mouseXOverTimeline);
                    } else {
                        timelineActions.scrollByPixels(event.deltaX);
                    }
                    scope.$apply();
                }


                var dragged = false;
                var dragStarted = 0;
                var impetusInit = null;
                var preventClick = false;
                // Impetus is inertia drag library

                function scrollbarSliderDragStart(){ // Activate Impetus forcibly
                    impetusInit = null;
                    dragStarted = mouseXOverTimeline;
                    dragged = false;
                    timelineActions.scrollbarSliderDragStart(mouseXOverTimeline);
                }
                new Impetus({
                    source: $canvas.get(0),
                    friction: timelineConfig.intertiaFriction,
                    start:function(x, y, event){
                        viewportMouseDown(event);

                        updateMouseCoordinate(event);
                        dragStarted = mouseXOverTimeline;
                        impetusInit = x;

                        dragged = false;
                        if(mouseOverElements.scrollbarSlider){
                            timelineActions.scrollbarSliderDragStart(mouseXOverTimeline);
                        }
                        if(mouseOverElements.timeline){
                            timelineActions.timelineDragStart(mouseXOverTimeline);
                        }
                    },
                    update: function(x, y) {
                        if(impetusInit === null){
                            impetusInit = x;
                        }
                        var virtualMouseX = dragStarted + x - impetusInit;
                        dragged = timelineActions.scrollbarSliderDrag(virtualMouseX) ||
                                  timelineActions.timelineDrag (virtualMouseX) || dragged;
                    },

                    release:function(x,y){
                        if(dragged){
                            preventClick = true;
                            setTimeout(function(){
                                preventClick = false;
                            }, timelineConfig.animationDuration);
                        }
                    },
                    stop:function(x,y){
                        updateMouseCoordinate(null);
                        timelineActions.scrollbarSliderDragEnd();
                        timelineActions.timelineDragEnd();
                    }
                });


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

                    if(mouseOverElements.scrollbarSlider){
                        timelineActions.fullZoomOut();
                        return;
                    }
                    if(mouseOverElements.scrollbar){
                        timelineActions.animateScroll(scope.scaleManager.getScrollTarget(mouseXOverTimeline));
                        return;
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
                        scrollbarClickOrHold();
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
                    timelineActions.scrollingToCursorStop();
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

                //For touch screens
                viewport.on('touchstart', viewportMouseDown);
                viewport.on('touchmove', viewportMouseMove);
                viewport.on('touchstop', viewportMouseLeave);

                // Actual browser events handling

                /*
                    We use updateTimelineWidth here because the width of the timeline
                    changes from when the page loads. When the page loads the volume button
                    is initially there. Then ng-class is applied hiding the button and
                    adjusting the width of the timeline. After this the timeline is longer
                    than the previous width causing the time marker to be offset.
                */
                scope.$watch('positionProvider',function(){
                    timelineActions.setPositionProvider(scope.positionProvider);
                    $timeout(updateTimelineWidth);
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
                function initRecordsProvider(){
                    scope.recordsProvider.init().then(function(hasArchive){
                        scope.emptyArchive = !hasArchive;
                        scope.loading = false;
                        if(hasArchive){
                            initTimeline(); // There is archive - init timeline
                        }else{ // No archive - wait and repeat attempt
                            $timeout(initRecordsProvider, Config.webclient.updateArchiveStateTimeout);
                        }
                    });
                }
                scope.$watch('recordsProvider',function(){ // RecordsProvider was changed - means new camera was selected
                    if(scope.recordsProvider) {
                        scope.loading = true;
                        initRecordsProvider();
                        timelineRender.setRecordsProvider(scope.recordsProvider);
                    }
                });

                // !!! Finally run required functions to initialize timeline
                updateTimelineHeight();
                /*
                    This timeout gets us the initial width. If volume is not disabled then
                    the width is correct. Otherwise we update the width in the positionProvider
                    watcher function. This is done when the postion provider changes because
                    the timeline's width should not change after that.
                */
                $timeout(updateTimelineWidth); // Adjust width.

                // !!! Start drawing
                animateScope.setDuration(timelineConfig.animationDuration);
                animateScope.setScope(scope);
                animateScope.start(render);

                //scope.scaleManager is set to null so that the garbage collecter cleans the object
                scope.$on('$destroy', function() {
                    $( window ).unbind('resize', updateTimelineWidth);
                    animateScope.stopScope(scope);
                    animateScope.stopHandler(render);
                    scope.scaleManager = null;
                });
            }
        };
    }]);

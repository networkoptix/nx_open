'use strict';

angular.module('webadminApp')
    .directive('timeline', ['$interval', function ($interval) {
        return {
            restrict: 'E',
            scope: {
                records: '='
                //start: '=',
                //end:   '=',
            },
            templateUrl: 'views/components/timeline.html',
            link: function (scope, element/*, attrs*/) {
                var timelineConfig = {
                    initialInterval: 1000*60*60*24*365, // no records - show last hour
                    futureInterval: 1000*60*60*24, // 24 hour to future - for timeline
                    zoomStep: 1.3, // Zoom speed
                    maxTimeLineWidth: 30000000, // maximum width for timeline, which browser can handle
                    // TODO: support changing boundaries on huge detailization
                    initialZoom: 0, // Начальный масштаб (степень) 0 - чтобы увидеть весь таймлайн.
                    updateInterval: 200, // Интервал обновления таймлайна
                    animationDuration: 200 // Скорость анимации
                };

                var viewportWidth = element.find('.viewport').width();

                function initTimeline(){
                    var now = (new Date()).getTime();
                    scope.start = scope.records? scope.records.start : (now - timelineConfig.initialInterval);
                    scope.end = now;
                    scope.ruler = new RulerModel(scope.start, scope.end + timelineConfig.futureInterval);
                    //scope.start = scope.ruler.start;

                    updateView();
                }

                function viewPortScroll(value){
                    return value?element.find('.viewport').scrollLeft(value):element.find('.viewport').scrollLeft();
                }

                function zoomLevel(){
                    return Math.pow(timelineConfig.zoomStep,scope.actualZoomLevel);
                }

                function initialWidth(){
                    return viewportWidth * zoomLevel();
                }

                function positionToDate(position){
                    return Math.round(scope.start + (scope.frameEnd - scope.start) * position / scope.frameWidth);
                }

                function dateToPosition(date){
                    return (date - scope.start) * initialWidth() / (scope.end - scope.start);
                }


                function updateActualLevel(){
                    //3. Select actual level
                    var secondsPerPixel = (scope.end-scope.start)/initialWidth()/1000;
                    for(var i=1; i < RulerModel.levels.length ; i ++ ){
                        if((RulerModel.levels[i].interval.getSeconds() / RulerModel.levels[i].width) < secondsPerPixel){
                            break;
                        }
                    }
                    scope.actualLevel = i-1;
                }
                function updateDetailization(){
                    var scrollStart = viewPortScroll();

                    var start = positionToDate (Math.max(0,scrollStart - viewportWidth));
                    var end = positionToDate (Math.min(scope.frameEnd,scrollStart + 2 * viewportWidth));

                    //4. Set interval for events
                    //TODO: Set interval for events
                    //console.warn('Set interval for events', new Date(start), new Date(end), scope.actualLevel);

                    //5. Set interval for ruler
                    scope.ruler.setInterval(start,end,scope.actualLevel);
                }

                function updateView(doAnimate){
                    //1. Calculate timelineWidth, frameWidth according to actualZoom
                    var initialWidth = viewportWidth * zoomLevel();// Width for initial timeline interval - base for zooming

                    var oldwidth = scope.timelineWidth;
                    var oldstart = scope.timelineStart;
                    var oldframe = scope.frameWidth;

                    scope.frameEnd = (new Date()).getTime();
                    scope.timelineWidth = Math.round(initialWidth * (scope.ruler.marksTree.end -  scope.ruler.marksTree.start)/(scope.end - scope.start));
                    scope.timelineStart = Math.round(initialWidth * (scope.ruler.marksTree.start - scope.start)/(scope.end - scope.start));
                    scope.frameWidth = Math.round(initialWidth * (scope.frameEnd -  scope.start)/(scope.end - scope.start));

                    if(oldwidth !== scope.timelineWidth || oldstart !== scope.timelineStart) {
                        element.find('.timeline').stop(true, false).animate({
                            width: scope.timelineWidth,
                            left: scope.timelineStart
                        }, doAnimate ? timelineConfig.animationDuration : 0);
                    }
                    if(oldframe !== scope.frameWidth) {
                        element.find('.frame').stop(true, false).animate({
                            width: scope.frameWidth
                        }, doAnimate ? timelineConfig.animationDuration : 0);
                    }

                    updateActualLevel();
                    updateDetailization();
                }

                scope.levels = RulerModel.levels;
                scope.actualZoomLevel = timelineConfig.initialZoom;
                scope.disableZoomOut = true;
                scope.disableZoomIn = false;
                scope.zoom = function(zoomIn, speed, targetScrollTime, targetScrollTimePosition){

                    speed = speed || 1;
                    if(!targetScrollTime){
                        targetScrollTime = positionToDate(viewPortScroll() + viewportWidth/2);
                        targetScrollTimePosition = 0.5;
                    }

                    if(zoomIn && !scope.disableZoomIn) {
                        scope.actualZoomLevel += speed;
                    }

                    if(!zoomIn && !scope.disableZoomOut ) {
                        scope.actualZoomLevel -= speed;
                        if(scope.actualZoomLevel <= 0) {
                            scope.actualZoomLevel = 0;
                        }
                    }

                    var newScrollPosition = dateToPosition(targetScrollTime) - viewportWidth * targetScrollTimePosition;

                    updateView(true);

                    element.find('.viewport').stop(true,false).animate({
                        scrollLeft:newScrollPosition
                    },timelineConfig.animationDuration);

                    scope.disableZoomOut = scope.actualZoomLevel<=0;

                    scope.disableZoomIn = scope.timelineWidth >= timelineConfig.maxTimeLineWidth || scope.actualLevel >= RulerModel.levels.length-1;
                };

                scope.$watch('records',initTimeline);

                var upadateInterval = $interval(updateView,timelineConfig.updateInterval);
                scope.$on('$destroy', function() { upadateInterval.cancel(); });

            }
        };
    }]);

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
            link: function (scope, element, attrs) {
                var timelineConfig = {
                    initialInterval: 1000*60, // no records - show last hour
                    futureInterval: 1000*60*60*24, // 24 hour to future - for timeline
                    zoomStep: 1.3, // Zoom speed
                    maxTimeLineWidth: 250000, // maximum width for timeline, which browser can handle
                    // TODO: support changing boundaries on huge detailization
                    initialZoom: 0, // Начальный масштаб (степень) 0 - чтобы увидеть весь таймлайн.
                    updateInterval: 1000, // Интервал обновления таймлайна
                    animationDuration: 200 // Скорость анимации
                };

                function initTimeline(){
                    var now = (new Date()).getTime();
                    scope.start = scope.records? scope.records.start : (now - timelineConfig.initialInterval);
                    scope.end = now;
                    scope.ruler = new RulerModel(scope.start, scope.end + timelineConfig.futureInterval);
                    //scope.start = scope.ruler.start;

                    updateView();
                    updateDetailization();
                }

                var viewportWidth = element.find('.viewport').width();

                function viewPortScroll(value){
                    return element.find('.viewport').scrollLeft(value);
                }

                function zoomLevel(){
                    return Math.pow(timelineConfig.zoomStep,scope.actualZoomLevel);
                }
                
                function positionToDate(position){
                    return Math.round(scope.start + (scope.end - scope.start) * position / (viewportWidth * zoomLevel()));
                }

                function dateToPosition(date){
                    return (date - scope.start) * (viewportWidth * zoomLevel()) / (scope.end - scope.start);
                }
                
                function initialWidth(){
                    return viewportWidth * zoomLevel();
                }
                
                function updateDetailization(){
                    var scrollStart = viewPortScroll();
                    var start = positionToDate (scrollStart);
                    var end = positionToDate (scrollStart + viewportWidth);
                    
                    //3. Select actual level
                    var secondsPerPixel = (scope.end-scope.start)/initialWidth/1000;
                    for(var i=1; i < RulerModel.levels.length ; i ++ ){
                        if((RulerModel.levels[i].interval.getSeconds() / RulerModel.levels[i].width) < secondsPerPixel){
                            break;
                        }
                    }
                    scope.actualLevel = i-1;

                    //4. Set interval for events
                    console.warn('Set interval for events', start,end,scope.actualLevel);

                    //5. Set interval for ruler
                    scope.ruler.setInterval(start,end,scope.actualLevel);
                }

                function updateView(){
                    //1. Calculate timelineWidth, frameWidth according to actualZoom
                    var initialWidth = initialWidth();// Width for initial timeline interval - base for zooming

                    scope.timelineWidth = initialWidth * (scope.ruler.marksTree.end -  scope.ruler.marksTree.start)/(scope.end - scope.start) ;
                    scope.timelineStart = initialWidth * (scope.ruler.marksTree.start - scope.start)/(scope.end - scope.start) ;
                    scope.frameWidth = initialWidth * ((new Date()).getTime() -  scope.start)/(scope.end - scope.start) ;

                    console.warn("run animate here");

                    element.find(".timeline").stop(true,false).animate({
                        width:scope.timelineWidth,
                        left:scope.timelineStart
                    },timelineConfig.animationDuration);

                    element.find(".frame").stop(true,false).animate({
                        width:scope.frameWidth
                    },timelineConfig.animationDuration);



                    console.warn("run updateDetailization() after animation complete");
                }

                scope.timelineWidth = '100%';
                scope.frameWidth = '100%';
                scope.levels = RulerModel.levels;
                scope.actualZoomLevel = timelineConfig.initialZoom;

                scope.$watch('records',initTimeline);

                var upadateInterval = $interval(updateView,timelineConfig.updateInterval);
                scope.$on('$destroy', function() {
                    upadateInterval.cancel();
                });


                scope.disableZoomOut = true;
                scope.disableZoomIn = false;


                scope.zoom = function(zoomIn, speed, targetScrollTime, targetScrollTimePosition){

                    speed = speed || 1;
                    if(!targetScrollTime){
                        targetScrollTime = positionToDate(viewPortScroll() + viewportWidth/2);
                        targetScrollTimePosition = 0.5
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

                    updateView();
                    
                    console.warn("run scroll animation here");
                    
                    updateDetailization();
                    
                    element.find(".viewport").stop(true,false).animate({
                        scrollLeft:newScrollPosition
                    },timelineConfig.animationDuration);

                    scope.disableZoomOut = scope.actualZoomLevel<=0;

                    scope.disableZoomIn = scope.frameWidth >= timelineConfig.maxTimeLineWidth || scope.actualLevel >= RulerModel.levels.length-1;
                };
            }
        };
    }]);

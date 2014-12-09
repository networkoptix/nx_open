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
                    updateInterval: 1000 // Интервал обновления таймлайна
                };

                function initTimeline(){
                    var now = (new Date()).getTime();
                    scope.start = scope.records? scope.records.start : (now - timelineConfig.initialInterval);
                    scope.end = now;
                    scope.ruler = new RulerModel(scope.start, scope.end + timelineConfig.futureInterval);
                    //scope.start = scope.ruler.start;

                    updateView();
                }

                var viewportWidth = element.find('.viewport').width();
                function updateView(){
                    //1. Calculate timelineWidth, frameWidth according to actualZoom
                    var zoomLevel = Math.pow(timelineConfig.zoomStep,scope.actualZoomLevel);
                    var initialWidth = viewportWidth * zoomLevel; // Width for initial timeline interval - base for zooming

                    scope.timelineWidth = initialWidth * (scope.ruler.marksTree.end -  scope.ruler.marksTree.start)/(scope.end - scope.start) + 'px';
                    scope.timelineStart = initialWidth * (scope.ruler.marksTree.start - scope.start)/(scope.end - scope.start) + 'px';

                    scope.frameWidth = initialWidth * ((new Date()).getTime() -  scope.start)/(scope.end - scope.start) + 'px';

                    console.log('updateView',zoomLevel,initialWidth,(scope.end - scope.start),(scope.ruler.end -  scope.ruler.start),scope.timelineWidth,scope.timelineStart,scope.frameWidth);

                    //2. Calculate active boundaries for detailization
                    console.warn('Calculate active boundaries for detailization');

                    // Find scrollstart as the time
                    var scrollStart = element.find('.viewport').scrollLeft();

                    console.log('scroll ',scrollStart);

                    var start = Math.round(scope.start + (scope.end - scope.start) * scrollStart / initialWidth);
                    var end = Math.round(scope.start + (scope.end - scope.start) * (scrollStart + viewportWidth)/ initialWidth);
                    // Find scrollstart + width as the time
                    console.log('visible time', new Date(start),new Date(end));

                    //3. Select actual level
                    var secondsPerPixel = (scope.end-scope.start)/initialWidth/1000;

                    for(var i=1; i < RulerModel.levels.length ; i ++ ){
                        if((RulerModel.levels[i].interval.getSeconds() / RulerModel.levels[i].width) < secondsPerPixel){
                            break;
                        }
                    }

                    console.log('test level', i, RulerModel.levels[i].name,
                            RulerModel.levels[i].interval.getSeconds() / RulerModel.levels[i].width, secondsPerPixel);

                    scope.actualLevel = i-1;

                    console.log('level',scope.actualLevel, RulerModel.levels[scope.actualLevel].name,
                        RulerModel.levels[scope.actualLevel].interval.getSeconds() / RulerModel.levels[scope.actualLevel].width, secondsPerPixel);

                    //4. Set interval for events
                    console.warn('Set interval for events', start,end,scope.actualLevel);

                    //5. Set interval for ruler
                    scope.ruler.setInterval(start,end,scope.actualLevel);
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
                scope.zoom = function(zoomIn){
                    if(zoomIn && !scope.disableZoomIn) {
                        scope.actualZoomLevel ++;
                    }

                    if(!zoomIn && !scope.disableZoomOut ) {
                        scope.actualZoomLevel --;
                        if(scope.actualZoomLevel <= 0) {
                            scope.actualZoomLevel = 0;
                        }
                    }

                    updateView();

                    scope.disableZoomOut = scope.actualZoomLevel<=0;

                    scope.disableZoomIn = scope.frameWidth >= timelineConfig.maxTimeLineWidth || scope.actualLevel >= RulerModel.levels.length-1;
                };
            }
        };
    }]);

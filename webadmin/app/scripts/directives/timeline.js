'use strict';

angular.module('webadminApp')
    .directive('timeline', function () {
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
                    initialInterval: 1000*60*60, // no records - show last hour
                    futureInterval: 1000*60*60*24, // 24 hour to future - for timeline
                    zoomStep: 1.3, // Zoom speed
                    maxTimeLineWidth: 250000, // maximum width for timeline, which browser can handle
                    // TODO: support changing boundaries on huge detailization
                    initialZoom: 0 // Начальный масштаб (степень) 0 - чтобы увидеть весь таймлайн.
                };

                function initTimeline(){
                    var now = (new Date()).getTime();
                    scope.start = scope.records? scope.records.start : (now - timelineConfig.initialInterval);
                    scope.end = now;
                    scope.ruler = new RulerModel(scope.start, scope.end + timelineConfig.futureInterval);
                    //scope.start = scope.ruler.start;

                    updateView();
                }

                var viewportWidth = element.find(".viewport").width();
                function updateView(){
                    //1. Calculate timelineWidth, frameWidth according to actualZoom
                    var zoomLevel = Math.pow(timelineConfig.zoomStep,scope.actualZoomLevel);
                    var initialWidth = viewportWidth * zoomLevel;

                    console.log("interval to show",new Date(scope.start),new Date(scope.end));

                    scope.timelineWidth = initialWidth * (scope.ruler.end -  scope.ruler.start)/(scope.end-scope.start) + 'px';

                    scope.timelineStart = initialWidth * (scope.ruler.start - scope.start)/(scope.end-scope.start) + 'px';

                    console.log(scope.timelineStart,scope.timelineWidth);

                    console.warn("Update frameWidth with time");
                    scope.frameWidth = scope.timelineWidth;//initialWidth + 'px';

                    console.log(
                        scope.ruler.end, scope.ruler.start,
                        scope.end, scope.start,
                        scope.ruler.end -  scope.ruler.start ,  scope.end-scope.start,
                        (scope.ruler.end -  scope.ruler.start)/(scope.end-scope.start),
                        scope.timelineWidth );


                    //2. Calculate active boundaries for detailization
                    console.warn("Calculate active boundaries for detailization");
                    var start = scope.start;
                    var end = scope.end;

                    //3. Select actual level
                    var secondsPerPixel = (scope.end-scope.start)/initialWidth/1000;

                    for(var i=1; i < RulerModel.levels.length ; i ++ ){
                        if((RulerModel.levels[i].interval.getSeconds() / RulerModel.levels[i].width) < secondsPerPixel){
                            break;
                        }
                    }

                    console.log("test level", i, RulerModel.levels[i].name,
                            RulerModel.levels[i].interval.getSeconds() / RulerModel.levels[i].width, secondsPerPixel);

                    scope.actualLevel = i-1;

                    console.log("level",scope.actualLevel, RulerModel.levels[scope.actualLevel].name,
                        RulerModel.levels[scope.actualLevel].interval.getSeconds() / RulerModel.levels[scope.actualLevel].width, secondsPerPixel);

                    //4. Set interval for events
                    console.warn("Set interval for events", scope.actualLevel);

                    //5. Set interval for ruler
                    scope.ruler.setInterval(start,end,scope.actualLevel);
                }

                scope.timelineWidth = '100%';
                scope.frameWidth = '100%';
                scope.levels = RulerModel.levels;
                scope.actualZoomLevel = timelineConfig.initialZoom;

                scope.$watch('records',initTimeline);
            }
        };
    });

'use strict';

angular.module('webadminApp')
    .directive('timeline2', ['$interval','animateScope', function ($interval,animateScope) {
        return {
            restrict: 'E',
            scope: {
                records: '='
                //start: '=',
                //end:   '=',
            },
            templateUrl: 'views/components/timeline2.html',
            link: function (scope, element/*, attrs*/) {
                var timelineConfig = {
                    initialInterval: 1000*60*60*24*365, // no records - show last hour
                    futureInterval: 1000*60*60*24, // 24 hour to future - for timeline
                    zoomStep: 1.3, // Zoom speed
                    maxTimeLineWidth: 30000000, // maximum width for timeline, which browser can handle
                    // TODO: support changing boundaries on huge detailization
                    initialZoom: 0, // Начальный масштаб (степень) 0 - чтобы увидеть весь таймлайн.
                    updateInterval: 20, // Интервал обновления таймлайна
                    animationDuration: 200, // Скорость анимации
                    rulerColor:'#bbbbbb', //Цвет линейки и подписи
                    rulerFontSize: 10, // Размер надписи на линейке
                    rulerBasicSize: 5, //Базовая высота штриха на линейке
                    rulerLabelPadding: 5, // Отступ в линейке над надписью
                    minimumMarkWidth: 5 // Ширина, при которой появлется новый уровень отметок на линейке
                };

                var viewportWidth = element.find('.viewport').width();
                scope.actualWidth = viewportWidth;

                var canvas = element.find('canvas').get(0);
                canvas.width  = viewportWidth;
                canvas.height = element.find('canvas').height();

                function initTimeline(){
                    var now = (new Date()).getTime();
                    scope.start = scope.records? scope.records.start : (now - timelineConfig.initialInterval);
                    scope.end = now;
                    //scope.ruler = new RulerModel(scope.start, scope.end + timelineConfig.futureInterval);
                    //scope.start = scope.ruler.start;

                    updateView();
                }

                function viewPortScroll(value){
                    return value?element.find('.scroll').scrollLeft(value):element.find('.scroll').scrollLeft();
                }

                function zoomLevel(zoom){
                    if(typeof(zoom)==='undefined') {
                        zoom = scope.actualZoomLevel;
                    }
                    return Math.pow(timelineConfig.zoomStep,zoom);
                }

                function initialWidth(zoom){
                    if(typeof(zoom) === 'undefined'){
                        return scope.actualWidth;
                    }
                    return viewportWidth * zoomLevel(zoom);
                }

                function secondsPerPixel () {
                    return (scope.end - scope.start) / initialWidth() / 1000;
                }
                function updateActualLevel(){
                    var oldLevel = scope.actualLevel;
                    //3. Select actual level
                    var secsPerPixel = secondsPerPixel();
                    for(var i=1; i < RulerModel.levels.length ; i ++ ){
                        if((RulerModel.levels[i].interval.getSeconds() / secsPerPixel) < timelineConfig.minimumMarkWidth){
                            break;
                        }
                    }
                    scope.actualLevel = i-1;
                    if(oldLevel !== scope.actualLevel){
                        // Level up - run animation for appearance of new level
                    }
                }

                function positionToDate(position){
                    return Math.round(scope.start + (scope.frameEnd - scope.start) * position / scope.frameWidth);
                }

                function dateToPosition(date,zoom){
                    return (date - scope.start) * initialWidth(zoom) / (scope.end - scope.start);
                }

                function dateToScreenPosition(date){
                    return dateToPosition(date) - viewPortScroll();
                }

                function screenPositionToDate(position){
                    return positionToDate(position + viewPortScroll());
                }

                function updateDetailization(){
                    /*var scrollStart = viewPortScroll();

                    var start = positionToDate (Math.max(0,scrollStart));
                    var end = positionToDate (Math.min(scope.frameEnd,scrollStart + viewportWidth));
*/
                    //4. Set interval for events
                    //TODO: Set interval for events
                    //console.warn('Set interval for events', new Date(start), new Date(end), scope.actualLevel);

                    //5. Set interval for ruler
                    //scope.ruler.setInterval(start,end,scope.actualLevel);
                }

                function drawMark(context, coordinate, level, label){

                    if(coordinate<0) {
                        return;
                    }

                    var size = Math.min(level+1,4) * timelineConfig.rulerBasicSize;

                    context.beginPath();
                    context.moveTo(coordinate, 0);
                    context.lineTo(coordinate, size);
                    context.stroke();

                    if(typeof(label)!=='undefined'){
                        var textSize = context.measureText(label);
                        var width = textSize.width;
                        context.fillText(label,coordinate - width/2, size + timelineConfig.rulerFontSize + timelineConfig.rulerLabelPadding);
                    }
                }

                function drawRuler(){

                    //1. Создаем контекст рисования
                    var context = canvas.getContext('2d');
                    context.fillStyle = timelineConfig.rulerColor;
                    context.strokeStyle = timelineConfig.rulerColor;
                    context.font = timelineConfig.rulerFontSize + 'px';

                    context.clearRect(0, 0, canvas.width, canvas.height);
                    var secsPerPixel  = secondsPerPixel();

                    //2. C учетом масштаба квантуем видимый интервал (выраваниваем начало назад, потом добавляем интервал сколько нужно раз - отрицательные координаты пропускаем)
                    var level = RulerModel.levels[scope.actualLevel];
                    var scrollStart = viewPortScroll();

                    var end = level.interval.alignToFuture (positionToDate (scrollStart + viewportWidth));
                    var position = level.interval.alignToPast(positionToDate (Math.max(0,scrollStart)));


                    var findLevel = function(level){
                        return level.interval.checkDate(position);
                    };
                    while(position<end){
                        var screenPosition = dateToScreenPosition(position);

                        //Detect the best level for this position
                        var markLevel = _.find(RulerModel.levels,findLevel);

                        var markLevelIndex = RulerModel.levels.indexOf(markLevel);

                        var label = ((markLevel.interval.getSeconds() / secsPerPixel) < markLevel.width )? '' : dateFormat(new Date(position), markLevel.format);

                        //3. Draw a mark
                        drawMark(context, screenPosition, scope.actualLevel - markLevelIndex , label);

                        position = level.interval.addToDate(position );
                    }
                }
                function updateView(doAnimate){
                    animateScope.process();

                    updateActualLevel();

                    var oldframe = scope.frameWidth;
                    scope.frameEnd = (new Date()).getTime();

                    scope.frameWidth = Math.round(initialWidth() * (scope.frameEnd -  scope.start)/(scope.end - scope.start));

                    //Draw ruler
                    drawRuler();

                    if(oldframe !== scope.frameWidth) {
                        element.find('.frame').stop(true, false).animate({
                            width: scope.frameWidth
                        }, doAnimate ? timelineConfig.animationDuration : 0);
                    }

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

                    var targetZoom = scope.actualZoomLevel;

                    if(zoomIn && !scope.disableZoomIn) {
                        targetZoom += speed;
                    }

                    if(!zoomIn && !scope.disableZoomOut ) {
                        targetZoom -= speed;
                        if(targetZoom <= 0) {
                            targetZoom = 0;
                        }
                    }

                    scope.actualZoomLevel = targetZoom;

                    var targetWidth = initialWidth(targetZoom);

                    scope.scrollPosition = viewPortScroll();
                    var newScrollPosition = dateToPosition(targetScrollTime,targetZoom) - viewportWidth * targetScrollTimePosition;
                    animateScope.animate(scope,'actualWidth',targetWidth,timelineConfig.animationDuration);
                    animateScope.animate(scope,'scrollPosition',newScrollPosition,timelineConfig.animationDuration,function(position){
                        viewPortScroll(position);
                    });

                    scope.disableZoomOut =  targetZoom <= 0;

                    console.warn('fix disablezoomin calculation');

                    scope.disableZoomIn = scope.timelineWidth >= timelineConfig.maxTimeLineWidth || scope.actualLevel >= RulerModel.levels.length-1;
                };

                scope.$watch('records',initTimeline);

                window.animationFrame = (function(callback) {
                    return window.requestAnimationFrame ||
                        window.webkitRequestAnimationFrame ||
                        window.mozRequestAnimationFrame ||
                        window.oRequestAnimationFrame ||
                        window.msRequestAnimationFrame ||
                        function(callback) {
                            window.setTimeout(callback, timelineConfig.updateInterval);
                        };
                })();

                var runAnimation = true;
                function animationFunction(){
                    updateView();
                    if(runAnimation) {
                        window.animationFrame(animationFunction);
                    }
                }
                initTimeline();
                animationFunction();
                scope.$on('$destroy', function() { runAnimation = false; });
            }
        };
    }]);

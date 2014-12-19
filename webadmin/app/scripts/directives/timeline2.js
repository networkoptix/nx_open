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
                    animationDuration: 400, // Скорость анимации, 200-400 прикольно
                    rulerColor:[187,187,187], //Цвет линейки и подписи
                    rulerFontSize: 9, // Размер надписи на линейке
                    rulerFontStep: 1, // Шаг увеличения надписи на линейке
                    rulerBasicSize: 6, //Базовая высота штриха на линейке
                    rulerStepSize: 3,// Шаг увеличения штрихов
                    rulerLabelPadding: 0.3, // Отступ в линейке над надписью
                    minimumMarkWidth: 5 // Ширина, при которой появлется новый уровень отметок на линейке
                };

                var viewportWidth = element.find('.viewport').width();
                var canvas = element.find('canvas').get(0);
                canvas.width  = viewportWidth;
                canvas.height = element.find('canvas').height();

                var frame = element.find('.frame');

                function initTimeline(){
                    var now = (new Date()).getTime();
                    scope.start = scope.records? scope.records.start : (now - timelineConfig.initialInterval);
                    scope.end = now;
                    //scope.ruler = new RulerModel(scope.start, scope.end + timelineConfig.futureInterval);
                    //scope.start = scope.ruler.start;

                    updateView();
                }

                var scroll = element.find('.scroll');
                function viewPortScroll(value){
                    return value?scroll.scrollLeft(value):scroll.scrollLeft();
                }

                function formatColor(color,alpha){
                    var color =  'rgba(' +
                        Math.round(color[0] /* * alpha + 255 * (1 - alpha) */) + ',' +
                        Math.round(color[1] /* * alpha + 255 * (1 - alpha) */) + ',' +
                        Math.round(color[2] /* * alpha + 255 * (1 - alpha) */) + ',' +
                        alpha + ')';

                    return color;
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
                    var oldLabelLevel = scope.visibleLabelsLevel;

                    //3. Select actual level
                    var secsPerPixel = secondsPerPixel();
                    for(var i=1; i < RulerModel.levels.length ; i ++ ){
                        var level = RulerModel.levels[i];

                        if((level.interval.getSeconds() / secsPerPixel) >= level.width){
                            scope.visibleLabelsLevel = i;
                        }

                        if((level.interval.getSeconds() / secsPerPixel) < timelineConfig.minimumMarkWidth){
                            break;
                        }
                    }
                    scope.actualLevel = i-1;
                    if(oldLevel < scope.actualLevel){ // Level up - run animation for appearance of new level
                        animateScope.progress(scope,'levelAppearance',timelineConfig.animationDuration);
                    }else if(oldLevel < scope.actualLevel){
                        animateScope.progress(scope,'levelDisappearance ',timelineConfig.animationDuration);
                    }

                    if(oldLabelLevel < scope.visibleLabelsLevel){ //Visible label level changed
                        animateScope.progress(scope,'labelAppearance',timelineConfig.animationDuration);
                    }else if(oldLabelLevel > scope.visibleLabelsLevel){
                        animateScope.progress(scope,'labelDisappearance',timelineConfig.animationDuration);
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

                function drawMark(context, coordinate, level, labelLevel, label){

                    if(coordinate<0) {
                        return;
                    }

                    var size = timelineConfig.rulerBasicSize;

                    if(level === 0){
                        size = timelineConfig.rulerBasicSize * scope.levelAppearance;
                    }else{
                        if(level<0){
                            size = timelineConfig.rulerBasicSize * (1-scope.levelDisappearance);
                        }else {
                            size = (Math.min(level, 4) + scope.levelEncreasing) * timelineConfig.rulerStepSize + timelineConfig.rulerBasicSize;
                        }
                    }

                    if(size <= 0){
                        return;
                    }


                    var color = formatColor(timelineConfig.rulerColor,1);
                    context.strokeStyle = color;
                    context.beginPath();
                    context.moveTo(coordinate, 0);
                    context.lineTo(coordinate, size);
                    context.stroke();

                    if(typeof(label)!=='undefined'){

                        var fontSize = timelineConfig.rulerFontSize + timelineConfig.rulerFontStep * Math.min(level,3);

                        if(scope.labelAppearance < 1 && labelLevel === 0) {
                            color = formatColor(timelineConfig.rulerColor, scope.labelAppearance);
                        }else if(scope.labelDisappearance < 1 && labelLevel < 0){
                            color = formatColor(timelineConfig.rulerColor, 1 - scope.labelDisappearance);
                        }

                        /*if(level === 0){
                            fontSize = timelineConfig.rulerBasicSize * scope.levelAppearance;
                        }else{
                            fontSize = (Math.min(level,2) + scope.levelEncreasing) * timelineConfig.rulerStepSize + timelineConfig.rulerBasicSize ;
                        }*/

                        context.font = fontSize  + 'px sans-serif';
                        context.fillStyle = color;

                        var width =  context.measureText(label).width;
                        context.fillText(label,coordinate - width/2, size + fontSize*(1 + timelineConfig.rulerLabelPadding));
                    }
                }

                function drawRuler(){

                    //1. Создаем контекст рисования
                    var context = canvas.getContext('2d');
                    context.fillStyle = formatColor(timelineConfig.rulerColor,1);

                    context.clearRect(0, 0, canvas.width, canvas.height);

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

                        var label = (markLevelIndex > scope.visibleLabelsLevel && !(scope.labelDisappearance<1 && markLevelIndex === scope.visibleLabelsLevel + 1))?
                            '' : dateFormat(new Date(position), markLevel.format);

                        //3. Draw a mark
                        drawMark(context, screenPosition, scope.actualLevel - markLevelIndex, scope.visibleLabelsLevel - markLevelIndex,label);

                        if(scope.levelDisappearance<1){
                            var smallposition = position;

                            var nextposition =level.interval.addToDate(position );
                            var smallLevel = RulerModel.levels[scope.actualLevel+1];
                            while(smallposition <nextposition ){
                                smallposition = smallLevel.interval.addToDate(smallposition);
                                screenPosition = dateToScreenPosition(smallposition);
                                drawMark(context, screenPosition, -1); // Draw hiding position
                            }
                        }

                        position = level.interval.addToDate(position );
                    }
                }
                function updateView(){

                    updateActualLevel();

                    var oldframe = scope.frameWidth;
                    scope.frameEnd = (new Date()).getTime();

                    scope.frameWidth = Math.round(initialWidth() * (scope.frameEnd -  scope.start)/(scope.end - scope.start));

                    //Draw ruler
                    drawRuler();

                    if(oldframe !== scope.frameWidth) {
                        frame.width(scope.frameWidth);
                    }

                    updateDetailization();
                }

                scope.levels = RulerModel.levels;
                scope.actualZoomLevel = timelineConfig.initialZoom;
                scope.disableZoomOut = true;
                scope.disableZoomIn = false;


                //animation parameters
                scope.scrollPosition = 0;
                scope.actualWidth = viewportWidth;
                scope.levelAppearance = 1; // Appearing new marks level
                scope.levelDisappearance = 1;// Hiding mark from oldlevel
                scope.levelEncreasing = 1; // Encreasing existing mark level
                scope.levelDecreasing = 1; // Encreasing existing mark level
                scope.labelAppearance = 1; // Labels become visible smoothly
                scope.labelDisappearance = 1; // Labels become visible smoothly

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

                initTimeline();

                animateScope.start(updateView);
                scope.$on('$destroy', function() { animateScope.stop(); });
            }
        };
    }]);

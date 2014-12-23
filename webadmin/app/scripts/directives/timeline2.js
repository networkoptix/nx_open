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
                    initialZoom: 0, // Initial zoom step 0 - to see whole timeline without scroll.
                    updateInterval: 20, // Animation interval
                    animationDuration: 400, // 200-400 for smooth animation
                    rulerColor:[0,0,0], //Color for ruler marks and labels
                    rulerFontSize: 9, // Smallest font size for labels
                    rulerFontStep: 1, // Step for increasing font size
                    rulerBasicSize: 6, // Size for smallest mark on ruler
                    rulerStepSize: 3,// Step for increasing marks
                    rulerLabelPadding: 0.3, // Padding between marks and labels (according to font size)
                    minimumMarkWidth: 5, // Minimum available width for smallest marks on ruler
                    increasedMarkWidth: 10,// Minimum width for encreasing marks size
                    maximumScrollScale: 20,// Maximum scale for scroll frame inside viewport - according to viewport width
                    colorLevels: 6 // Number of different color levels
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
                    updateView();
                }

                var scroll = element.find('.scroll');


                function viewPortScrollRelative(value){
                    var currentScrollScale = scope.scrollWidth / viewportWidth;
                    if(typeof(value) ==='undefined') {
                        var position = scroll.scrollLeft() + viewportWidth/2;
                        return position / scope.scrollWidth;
                    } else {
                        var setValue = Math.round(Math.max(value * scope.scrollWidth - viewportWidth/2,0));
                        scroll.scrollLeft(setValue);
                    }
                }


                function blurColor(color,alpha){
                    /*
                    var colorString =  'rgba(' +
                        Math.round(color[0]) + ',' +
                        Math.round(color[1]) + ',' +
                        Math.round(color[2]) + ',' +
                        alpha + ')';
                    */
                    if(alpha>1)
                        alpha = 1;
                    var colorString =  'rgb(' +
                        Math.round(color[0] * alpha + 255 * (1 - alpha)) + ',' +
                        Math.round(color[1] * alpha + 255 * (1 - alpha)) + ',' +
                        Math.round(color[2] * alpha + 255 * (1 - alpha)) + ')';

                    return colorString;
                }

                function zoomLevel(){
                    return Math.pow(timelineConfig.zoomStep,scope.actualZoomLevel);
                }

                function secondsPerPixel () {
                    return (scope.end - scope.start) / scope.actualWidth / 1000;
                }

                function updateActualLevel(){
                    var oldLevel = scope.actualLevel;
                    var oldLabelLevel = scope.visibleLabelsLevel;
                    var oldEncreasedLevel = scope.increasedLevel;

                    //3. Select actual level
                    var secsPerPixel = secondsPerPixel();
                    for(var i=1; i < RulerModel.levels.length ; i ++ ){
                        var level = RulerModel.levels[i];

                        var secondsPerLevel =(level.interval.getSeconds() / secsPerPixel);
                        if(secondsPerLevel >= level.width){
                            scope.visibleLabelsLevel = i;
                        }

                        if(secondsPerLevel > timelineConfig.increasedMarkWidth){
                            scope.increasedLevel = i;
                        }

                        if(secondsPerLevel < timelineConfig.minimumMarkWidth){
                            break;
                        }
                    }

                    scope.actualLevel = i-1;

                    if(oldLevel < scope.actualLevel){ // Level up - run animation for appearance of new level
                        animateScope.progress(scope,'levelAppearance',timelineConfig.animationDuration);
                    }else if(oldLevel > scope.actualLevel){
                        animateScope.progress(scope,'levelDisappearance',timelineConfig.animationDuration);
                    }

                    if(oldEncreasedLevel < scope.increasedLevel){
                        animateScope.progress(scope,'levelEncreasing',timelineConfig.animationDuration);
                    }else if(oldEncreasedLevel > scope.increasedLevel){
                        animateScope.progress(scope,'levelDecreasing',timelineConfig.animationDuration);
                    }

                    if(oldLabelLevel < scope.visibleLabelsLevel){ //Visible label level changed
                        animateScope.progress(scope,'labelAppearance',timelineConfig.animationDuration);
                    }else if(oldLabelLevel > scope.visibleLabelsLevel){
                        animateScope.progress(scope,'labelDisappearance',timelineConfig.animationDuration);
                    }

                }




                function screenStartPosition(){
                    return viewPortScrollRelative() * scope.frameWidth - viewportWidth/2;
                }

                function dateToScreenPosition(date){
                    var position = (date - scope.start) * scope.frameWidth / (scope.frameEnd - scope.start);
                    return position - screenStartPosition(0);
                }


                function screenPositionToDate(position){
                    //1. Get Relative Position and calculate it's date, related to frame
                    var totalInterval = (scope.frameEnd - scope.start);
                    var currentZoom = scope.actualWidth / viewportWidth;
                    var toret = scope.start +  totalInterval * (viewPortScrollRelative() + (position - 0.5) / currentZoom);
                    return Math.round(toret);
                }

                function updateDetailization(){
                    //4. Set interval for events
                    //TODO: Set interval for events
                    //console.warn('Set interval for events', new Date(start), new Date(end), scope.actualLevel);
                }

                function drawMark(context, coordinate, level, labelLevel, label){

                    if(coordinate<0) {
                        return;
                    }

                    var size = timelineConfig.rulerBasicSize;
                    var colorBlur = 0;

                    if(level === 0){
                        size = timelineConfig.rulerBasicSize * scope.levelAppearance;
                        colorBlur = scope.levelAppearance / timelineConfig.colorLevels;
                    }else if(level<0){
                        size = timelineConfig.rulerBasicSize * (1 - scope.levelDisappearance);
                        colorBlur =  (1 - scope.levelDisappearance) / timelineConfig.colorLevels;
                    }else {
                        var animationModifier = 1;
                        if(scope.levelAppearance < 1 ){
                            animationModifier = scope.levelAppearance;
                        }else if( scope.levelDisappearance < 1){
                            animationModifier = 2 - scope.levelDisappearance;
                        }
                        colorBlur = (Math.min(level, timelineConfig.colorLevels - 1) + animationModifier) / timelineConfig.colorLevels;
                        size = (Math.min(level, 4) + animationModifier ) * timelineConfig.rulerStepSize + timelineConfig.rulerBasicSize;
                    }


                    if(size <= 0){
                        return;
                    }


                    var color = blurColor(timelineConfig.rulerColor,colorBlur);
                    context.strokeStyle = color;
                    context.beginPath();
                    context.moveTo(coordinate, 0);
                    context.lineTo(coordinate, size);
                    context.stroke();

                    if(typeof(label)!=='undefined'){

                        var fontSize = timelineConfig.rulerFontSize + timelineConfig.rulerFontStep * Math.min(level,3);

                        if(scope.labelAppearance < 1 && labelLevel === 0) {
                            color = blurColor(timelineConfig.rulerColor, scope.labelAppearance * colorBlur);
                        }else if(scope.labelDisappearance < 1 && labelLevel < 0){
                            color = blurColor(timelineConfig.rulerColor, (1 - scope.labelDisappearance) * colorBlur);
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
                    //1. Create context for drawing
                    var context = canvas.getContext('2d');
                    context.fillStyle = blurColor(timelineConfig.rulerColor,1);

                    context.clearRect(0, 0, canvas.width, canvas.height);

                    //2. Align visible interval to current level (start to past, end to future)
                    var level = RulerModel.levels[scope.actualLevel];

                    var end = level.interval.alignToFuture (screenPositionToDate(1));
                    var position = level.interval.alignToPast(screenPositionToDate(0));

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
                            while(smallposition < nextposition ){
                                smallposition = smallLevel.interval.addToDate(smallposition);
                                screenPosition = dateToScreenPosition(smallposition);
                                drawMark(context, screenPosition, -1); // Draw hiding position
                            }
                        }
                        position = level.interval.addToDate(position);
                    }
                }
                function updateView(){
                    // Update boundaries to current moment
                    scope.frameEnd = (new Date()).getTime();
                    scope.frameWidth = Math.round(scope.actualWidth * (scope.frameEnd -  scope.start)/(scope.end - scope.start));

                    updateActualLevel();
                    drawRuler();
                    updateDetailization();
                }

                scope.levels = RulerModel.levels;
                scope.actualZoomLevel = timelineConfig.initialZoom;
                scope.disableZoomOut = true;
                scope.disableZoomIn = false;


                //animation parameters
                scope.scrollPosition = 0;
                scope.actualWidth = viewportWidth;
                scope.scrollWidth = viewportWidth;
                scope.levelAppearance   = 1; // Appearing new marks level
                scope.levelDisappearance = 1;// Hiding mark from oldlevel
                scope.levelEncreasing = 1; // Encreasing existing mark level
                scope.levelDecreasing = 1; // Encreasing existing mark level
                scope.labelAppearance = 1; // Labels become visible smoothly
                scope.labelDisappearance = 1; // Labels become visible smoothly

                scope.zoom = function(zoomIn, speed, targetScrollTime, targetScrollTimePosition){
                    speed = speed || 1;

                    if(zoomIn && !scope.disableZoomIn) {
                        scope.actualZoomLevel += speed;
                    }

                    if(!zoomIn && !scope.disableZoomOut ) {
                        scope.actualZoomLevel -= speed;
                        if(scope.actualZoomLevel <= 0) {
                            scope.actualZoomLevel = 0;
                        }
                    }

                    var targetWidth = viewportWidth * zoomLevel();
                    animateScope.animate(scope,'actualWidth',targetWidth,timelineConfig.animationDuration);

                    //We should keep relative position during resizing width
                    var scrollRelativePosition = viewPortScrollRelative();
                    animateScope.progress(scope,'scrollPosition',timelineConfig.animationDuration,function(){
                        var oldframe = scope.scrollWidth;
                        scope.scrollWidth = Math.round(Math.min( scope.frameWidth, timelineConfig.maximumScrollScale * viewportWidth));

                        if(oldframe !== scope.scrollWidth) {
                            frame.width(scope.scrollWidth);
                            viewPortScrollRelative(scrollRelativePosition);
                        }
                    });


                    scope.disableZoomOut =  scope.actualZoomLevel <= 0;

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

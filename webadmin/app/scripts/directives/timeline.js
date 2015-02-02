'use strict';

angular.module('webadminApp')
    .directive('timeline', ['$interval','animateScope', function ($interval,animateScope) {
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
                    zoomStep: 1.5, // Zoom speed
                    maxTimeLineWidth: 30000000, // maximum width for timeline, which browser can handle
                    // TODO: support changing boundaries on huge detailization
                    initialZoom: 0, // Initial zoom step 0 - to see whole timeline without scroll.
                    updateInterval: 20, // Animation interval
                    animationDuration: 300, // 200-400 for smooth animation
                    rulerColor:[0,0,0], //Color for ruler marks and labels
                    rulerFontSize: 9, // Smallest font size for labels
                    rulerFontStep: 1, // Step for increasing font size
                    rulerBasicSize: 6, // Size for smallest mark on ruler
                    rulerStepSize: 3,// Step for increasing marks
                    labelsPadding: 0.3, // Padding between marks and labels (according to font size)
                    minimumMarkWidth: 5, // Minimum available width for smallest marks on ruler
                    increasedMarkWidth: 10,// Minimum width for encreasing marks size
                    maximumScrollScale: 20,// Maximum scale for scroll frame inside viewport - according to viewport width
                    colorLevels: 6, // Number of different color levels
                    scrollingSpeed: 0.25,// One click to scroll buttons - scroll timeline by half of the screen
                    maxVerticalScrollForZoom: 5000, // value for adjusting zoom
                    scrollBoundariesPrecision: 0.000001, // Where we should disable right and left scroll buttons
                    dblClckZoomSpeed:2,// Zoom speed for dbl click
                    topLabelBorderColor: 'silver', // Color for borders for top labels
                    topLabelTextColor: 'rgb(128,128,128)', // Color for text for top labels
                    topLabelHeight: 12, // Size for top label text
                    topLabelPadding: 2, // Padding around top label

                    font:'sans-serif',
                    evenColor: 'rgb(200,200,255)',
                    oddColor: 'white',


                    chunkTopMargin:5,
                    chunkHeight:10,
                    exactChunkColor:'rgb(128,128,128)',
                    loadingChunkColor:'rgb(200,200,200)'

                };

                animateScope.setDuration(timelineConfig.animationDuration);
                animateScope.setScope(scope);

                var viewportWidth = element.find('.viewport').width();
                var containerHeight = element.find('.viewport').height();

                var frame = element.find('.frame');
                var scroll = element.find('.scroll');

                var canvas = element.find('canvas').get(0);
                canvas.width  = viewportWidth;

                var scrollBarWidth = getScrollbarWidth();
                canvas.height = containerHeight - scrollBarWidth;
                element.find('canvas').height(containerHeight - scrollBarWidth);

                // set up scroll up-down for zoom
                function getScrollbarWidth() {
                    var outer = document.createElement('div');
                    outer.style.visibility = 'hidden';
                    outer.style.width = '100px';
                    outer.style.msOverflowStyle = 'scrollbar'; // needed for WinJS apps

                    document.body.appendChild(outer);

                    var widthNoScroll = outer.offsetWidth;
                    // force scrollbars
                    outer.style.overflow = 'scroll';

                    // add innerdiv
                    var inner = document.createElement('div');
                    inner.style.width = '100%';
                    outer.appendChild(inner);

                    var widthWithScroll = inner.offsetWidth;

                    // remove divs
                    outer.parentNode.removeChild(outer);

                    return widthNoScroll - widthWithScroll;
                }

                function bound(min,value,max){
                    return Math.max(min,Math.min(value,max));
                }

                var userScroll = true;
                var scrollLeftRelativeValue = 0;
                function viewPortScrollRelative(value){
                    var availableWidth = scope.scrollWidth - viewportWidth;
                    if(typeof(value) === 'undefined') {
                        if(availableWidth < 1){
                            scrollLeftRelativeValue = 0.5;
                        }
                        return scrollLeftRelativeValue;
                    } else {
                        scrollLeftRelativeValue = value;
                        var scrollValue = Math.round(value * availableWidth);
                        userScroll = false;
                        scroll.scrollLeft(scrollValue);
                        userScroll = true;
                    }
                }

                scroll.scroll(function(){
                    if(!userScroll) {
                        return;
                    }
                    var scrollPos = scroll.scrollLeft();
                    var availableWidth = scope.scrollWidth - viewportWidth;
                    viewPortScrollRelative(scrollPos/availableWidth);
                });

                var targetZoomLevel = 1;

                scroll.mousewheel(function(event){
                    event.preventDefault();

                    if(Math.abs(event.deltaY) > Math.abs(event.deltaX)) { // Zoom or scroll - not both
                        if (scope.zooming === 1) { // if zoom animation is not going yet
                            scope.positionToKeep = event.offsetX/viewportWidth;

                            var actualZoomLevel = (scope.scrollZooming === 1)?scope.actualZoomLevel:targetZoomLevel;
                            var actualVerticalScrollForZoom = actualZoomLevel / scope.maxZoomLevel;
                            actualVerticalScrollForZoom -= event.deltaY / timelineConfig.maxVerticalScrollForZoom;

                            actualVerticalScrollForZoom = bound(0,actualVerticalScrollForZoom,1);

                            targetZoomLevel = scope.maxZoomLevel * actualVerticalScrollForZoom;

                            // If user uses touchpad - do not animate, just set value
                            // scope.actualZoomLevel = targetZoomLevel;
                            animateScope.animate(scope,'actualZoomLevel',targetZoomLevel);
                            animateScope.progress(scope,'scrollZooming');
                        }
                    }else {
                        var horScroll = viewPortScrollRelative();
                        horScroll += event.deltaX / scope.actualWidth;
                        viewPortScrollRelative(horScroll);
                    }

                    scope.$apply();

                });

                function maxZoomLevel(){
                    var maxLevel = RulerModel.levels[RulerModel.levels.length-1];
                    var targetSecPerPixel =  maxLevel.interval.getSeconds()/maxLevel.width;
                    var totalSeconds = (scope.end - scope.start) / 1000;
                    var maxLevelValue = totalSeconds / targetSecPerPixel / viewportWidth;
                    var maxZoomLevelResult = Math.ceil(Math.log(maxLevelValue) / Math.log(timelineConfig.zoomStep));
                    return maxZoomLevelResult;
                }

                function blurColor(color,alpha){
                    /*
                    var colorString =  'rgba(' +
                        Math.round(color[0]) + ',' +
                        Math.round(color[1]) + ',' +
                        Math.round(color[2]) + ',' +
                        alpha + ')';
                    */
                    if(alpha > 1) { // Not so good. It's hack. Somewhere
                        console.error('bad value', alpha);
                    }
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

                        if(typeof(level.topWidth)!=='undefined' &&
                            secondsPerLevel >= level.topWidth){
                            scope.topLabelsLevel = i;
                        }

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

                    scope.disableZoomIn  = Math.ceil(scope.actualZoomLevel) >= scope.maxZoomLevel;//scope.actualLevel >= RulerModel.levels.length-1;
                    scope.disableZoomOut = scope.actualZoomLevel <= 0;


                    if(oldLevel < scope.actualLevel){ // Level up - run animation for appearance of new level
                        animateScope.progress(scope,'levelAppearance');
                    }else if(oldLevel > scope.actualLevel){
                        animateScope.progress(scope,'levelDisappearance');
                    }

                    if(oldEncreasedLevel < scope.increasedLevel){
                        animateScope.progress(scope,'levelEncreasing');
                    }else if(oldEncreasedLevel > scope.increasedLevel){
                        animateScope.progress(scope,'levelDecreasing');
                    }

                    if(oldLabelLevel < scope.visibleLabelsLevel){ //Visible label level changed
                        animateScope.progress(scope,'labelAppearance');
                    }else if(oldLabelLevel > scope.visibleLabelsLevel){
                        animateScope.progress(scope,'labelDisappearance');
                    }


                }



                function screenStartPosition(){
                    return viewPortScrollRelative() * (scope.frameWidth - viewportWidth);
                }

                function dateToScreenPosition(date){
                    var position = scope.frameWidth * (date - scope.start) / (scope.frameEnd - scope.start);
                    return position - screenStartPosition();
                }

                function screenRelativePositionToDate(position){
                    var globalPosition = screenStartPosition() + position*viewportWidth;
                    var toret = Math.round(scope.start + (scope.frameEnd - scope.start)*globalPosition/scope.frameWidth);
                    return toret;
                }

                function screenRelativePositionAndDateToRelativePositionForScroll(date,screenRelativePosition){
                    if(scope.frameWidth === viewportWidth){
                        return 0;
                    }
                    var dateRelativePosition = (date - scope.start)/(scope.frameEnd - scope.start);
                    var datePosition = dateRelativePosition * scope.frameWidth;
                    var startPosition = datePosition - viewportWidth * screenRelativePosition;
                    var screenStartRelativePos = startPosition / (scope.frameWidth-viewportWidth);
                    return screenStartRelativePos;
                }


                function drawChunk(context,startCoordinate, endCoordinate, exactChunk){

                    console.log("drawChunk",startCoordinate, endCoordinate, exactChunk);

                    var vertStart = timelineConfig.topLabelHeight +
                        5 * timelineConfig.rulerStepSize + timelineConfig.rulerBasicSize
                        + timelineConfig.rulerFontSize + timelineConfig.rulerFontStep * 3
                        + timelineConfig.chunkTopMargin;

                    vertStart = 0;
                    timelineConfig.chunkHeight= 100;

                    context.fillStyle = exactChunk? timelineConfig.exactChunkColor:timelineConfig.loadingChunkColor;

                    context.fillRect(startCoordinate, vertStart , Math.max(1,endCoordinate - startCoordinate), timelineConfig.chunkHeight);
                    context.stroke();
                }

                function drawEvents(){
                    //4. Set interval for events
                    //TODO: Set interval for events

                    var context = canvas.getContext('2d');
                    var level = RulerModel.levels[scope.actualLevel];
                    var end = level.interval.alignToFuture(screenRelativePositionToDate(1));
                    var start = level.interval.alignToFuture(screenRelativePositionToDate(0));

                    console.log('Set interval for events', new Date(start), new Date(end), scope.actualLevel);

                    if(!!scope.records) {
                        // 1. Splice events
                        var events = scope.records.setInterval(  start, end, scope.actualLevel);

                        // 2. Draw em!
                        console.log("splice",scope.actualLevel,events);

                        for(var i=0;i<events.length;i++){
                            var chunk = events[i];
                            var startCoordinate =  dateToScreenPosition(chunk.start);
                            var endCoordinate =  dateToScreenPosition(chunk.end);

                            drawChunk(context,startCoordinate, endCoordinate,chunk.level);
                        }
                    }
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
                    context.moveTo(coordinate, timelineConfig.topLabelHeight);
                    context.lineTo(coordinate, size + timelineConfig.topLabelHeight);
                    context.stroke();

                    if(typeof(label)!=='undefined'){

                        var fontSize = timelineConfig.rulerFontSize + timelineConfig.rulerFontStep * Math.min(level,3);

                        if(scope.labelAppearance < 1 && labelLevel === 0) {
                            color = blurColor(timelineConfig.rulerColor, scope.labelAppearance * colorBlur);
                        }else if(scope.labelDisappearance < 1 && labelLevel < 0){
                            color = blurColor(timelineConfig.rulerColor, (1 - scope.labelDisappearance) * colorBlur);
                        }

                        context.font = fontSize  + 'px ' + timelineConfig.font;
                        context.fillStyle = color;

                        var width =  context.measureText(label).width;
                        context.fillText(label,coordinate - width/2, size + fontSize*(1 + timelineConfig.labelsPadding) + timelineConfig.topLabelHeight);
                    }
                }

                function drawTopLabel(context, startCoordinate, endCoordinate, label, odd){

                    startCoordinate = Math.max(startCoordinate,0);
                    endCoordinate = Math.min(endCoordinate,viewportWidth);

                    context.fillStyle = odd? timelineConfig.oddColor:timelineConfig.evenColor;

                    context.fillRect(startCoordinate, 0 , endCoordinate - startCoordinate, timelineConfig.topLabelHeight);
                    context.stroke();


                    var textSize = timelineConfig.topLabelHeight - timelineConfig.topLabelPadding;

                    context.font = textSize  + 'px ' + timelineConfig.font;
                    context.fillStyle = timelineConfig.topLabelTextColor;
                    var width =  context.measureText(label).width;

                    var coordinate = (startCoordinate + endCoordinate - width) / 2 ;

                    if(coordinate < 0){
                        coordinate = 0;
                    }
                    if(coordinate + width > endCoordinate - timelineConfig.topLabelPadding && endCoordinate < viewportWidth){
                        coordinate = endCoordinate - width - timelineConfig.topLabelPadding;
                    }

                    if(coordinate < startCoordinate + timelineConfig.topLabelPadding && startCoordinate > 0){
                        coordinate = startCoordinate + timelineConfig.topLabelPadding;
                    }

                    context.fillText(label, coordinate, timelineConfig.topLabelHeight - timelineConfig.topLabelPadding );

                }

                function drawRuler(){
                    //1. Create context for drawing
                    var context = canvas.getContext('2d');
                    context.fillStyle = blurColor(timelineConfig.rulerColor,1);

                    context.clearRect(0, 0, canvas.width, canvas.height);


                    //1. Drow top labels


                    context.strokeStyle = timelineConfig.topLabelBorderColor;

                    context.beginPath();
                    context.moveTo(0, timelineConfig.topLabelHeight);
                    context.lineTo(viewportWidth, timelineConfig.topLabelHeight);
                    context.stroke();

                    var level = RulerModel.levels[scope.topLabelsLevel];
                    var end = level.interval.alignToFuture(screenRelativePositionToDate(1));
                    var position = level.interval.alignToPast(screenRelativePositionToDate(0));


                    while(position<end){ // Draw labels above marks
                        var endPosition = level.interval.addToDate(position);
                        var screenStartPosition = dateToScreenPosition(position);
                        var screenEndPosition = dateToScreenPosition(endPosition);
                        var label = dateFormat(new Date(position), level.topFormat);
                        drawTopLabel(context, screenStartPosition, screenEndPosition, label, Math.round((position / 1000 / level.interval.getSeconds())) % 2 === 1);
                        position = endPosition;
                    }

                    //2. Align visible interval to current level (start to past, end to future)

                    level = RulerModel.levels[scope.actualLevel];
                    end = level.interval.alignToFuture(screenRelativePositionToDate(1));
                    position = level.interval.alignToFuture(screenRelativePositionToDate(0));

                    var findLevel = function(level){
                        return level.interval.checkDate(position);
                    };

                    while(position<end){ // Draw marks
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
                function updateBoundariesAndScroller(){
                    var keepDate = screenRelativePositionToDate(scope.positionToKeep); // Keep current timeposition for scrolling

                    scope.actualWidth = viewportWidth * zoomLevel();

                    // Update boundaries to current moment
                    scope.frameEnd = /*scope.end;//*/(new Date()).getTime();
                    scope.frameWidth = Math.round(scope.actualWidth * (scope.frameEnd -  scope.start)/(scope.end - scope.start));

                    //1. Update scroller width
                    var oldframe = scope.scrollWidth;
                    scope.scrollWidth = Math.round(Math.min( scope.frameWidth, timelineConfig.maximumScrollScale * viewportWidth));
                    if(oldframe !== scope.scrollWidth) { // Do not affect DOM if it is not neccessary
                        frame.width(scope.scrollWidth);
                    }


                    var newPosition = 0;
                    if(scope.scrolling < 1){ // animating scroll at the moment- should scroll
                        newPosition = scope.targetScrollPosition/ (scope.frameWidth - viewportWidth);

                    }else { // Otherwise - keep current position in case of changing width
                        newPosition = screenRelativePositionAndDateToRelativePositionForScroll(keepDate, scope.positionToKeep);

                        // TODO: In live mode - scroll right
                        // TODO: disable live if view was scrolled

                    }
                    newPosition = bound(0,newPosition,1);
                    viewPortScrollRelative(newPosition);
                    scope.checkRightLeftVisibility();
                }

                function updateView(){
                    updateBoundariesAndScroller();
                    updateActualLevel();
                    drawRuler();
                    drawEvents();
                }

                function initTimeline(){
                    var now = (new Date()).getTime();
                    scope.start = scope.records? scope.records.start : (now - timelineConfig.initialInterval);
                    scope.end = now;
                    scope.maxZoomLevel = maxZoomLevel();


                    console.log('initTimeline',scope.start,new Date(scope.start),scope.maxZoomLevel );
                    updateView();
                }



                scope.positionToKeep = 0.5;//Position on screen to keep while zooming
                scope.levels = RulerModel.levels;
                scope.actualZoomLevel = timelineConfig.initialZoom;
                scope.disableZoomOut = true;
                scope.disableZoomIn = false;
                scope.disableScrollRight = true;
                scope.disableScrollLeft = true;

                //animation parameters
                scope.actualWidth = viewportWidth;
                scope.scrollWidth = viewportWidth;
                scope.levelAppearance   = 1; // Appearing new marks level
                scope.levelDisappearance = 1;// Hiding mark from oldlevel
                scope.levelEncreasing = 1; // Encreasing existing mark level
                scope.levelDecreasing = 1; // Encreasing existing mark level
                scope.labelAppearance = 1; // Labels become visible smoothly
                scope.labelDisappearance = 1; // Labels become visible smoothly
                scope.zooming = 1; //zoom animation progress
                scope.scrolling = 1;//Scrolling to some position
                scope.targetScrollPosition = 0;
                scope.scrollZooming = 1; //Animate zoom while scroll

                scope.scroll = function(right,value){
                    if(scope.scrolling >= 1){
                        scope.targetScrollPosition = screenStartPosition();// Set up initial scroll position
                    }

                    var newPosition = scope.targetScrollPosition + (right ? 1 : -1) * value * viewportWidth;
                    // Here we have a problem:we should scroll left a bit if it is not .

                    newPosition = bound(0,newPosition,scope.frameWidth - viewportWidth);

                    animateScope.animate(scope,'targetScrollPosition',newPosition);
                    var animation = animateScope.progress(scope,'scrolling');
                    animation.then(function(){
                        scope.checkRightLeftVisibility(right);
                    });
                    return animation;
                };
                scope.scrollToEnd = function(right){
                    animateScope.animate(scope,'targetScrollPosition',right?scope.frameWidth:-scope.frameWidth);
                    var animation = animateScope.progress(scope,'scrolling');
                    animation.then(function(){
                        scope.checkRightLeftVisibility(right);
                    });
                    return animation;
                };

                scope.checkRightLeftVisibility = function(right){
                    var scroll = viewPortScrollRelative();
                    if(right || typeof(right) === 'undefined') {
                        scope.disableScrollRight = scroll > 1 - timelineConfig.scrollBoundariesPrecision || scope.frameWidth === viewportWidth;
                    }
                    if(!right){
                        scope.disableScrollLeft = scroll < timelineConfig.scrollBoundariesPrecision || scope.frameWidth === viewportWidth;
                    }
                };

                var scrollingNow = false;
                function scrollingProcess(right){
                    scope.scroll(right,timelineConfig.scrollingSpeed).then(function(){
                        if(scrollingNow) {
                            scrollingProcess(right);
                        }
                    });
                }
                scope.scrollStart = function(right){
                    // Run animation by animation.
                    scrollingNow = true;
                    scrollingProcess(right);
                };
                scope.scrollEnd = function(right){
                    scrollingNow = false;
                };

                scope.zoom = function(zoomIn,speed,keep){
                    scope.positionToKeep = keep || 0.5;
                    speed = speed || 1;

                    var targetZoomLevel = scope.actualZoomLevel;
                    if(zoomIn && !scope.disableZoomIn) {
                        targetZoomLevel += speed;
                    }

                    if(!zoomIn && !scope.disableZoomOut ) {
                        targetZoomLevel -= speed;
                    }

                    targetZoomLevel = bound(0,targetZoomLevel,scope.maxZoomLevel);
                    animateScope.animate(scope,'actualZoomLevel',targetZoomLevel);
                    animateScope.progress(scope,'zooming');

                };

                scope.zoomToPoint = function(offsetX){
                    scope.zoom(true,timelineConfig.dblClckZoomSpeed,(offsetX - scroll.scrollLeft())/ viewportWidth);
                };

                var draggingPosition = null;
                scope.dragStart = function(offsetX) {
                    var screenRelativePosition = (offsetX - scroll.scrollLeft())/ viewportWidth;
                    draggingPosition  = screenRelativePositionToDate(screenRelativePosition);
                };
                scope.drag = function(offsetX) {
                    if(draggingPosition === null){
                        return;
                    }
                    var screenRelativePosition = (offsetX - scroll.scrollLeft())/ viewportWidth;
                    var newPosition = screenRelativePositionAndDateToRelativePositionForScroll(draggingPosition, screenRelativePosition);
                    newPosition = bound(0,newPosition,1);
                    viewPortScrollRelative(newPosition);
                    scope.checkRightLeftVisibility();
                };
                scope.dragEnd = function() {
                    draggingPosition = null;
                };

                scope.$watch('records',function(){
                    if(scope.records) {
                        scope.records.ready.then(initTimeline);
                    }
                });

                initTimeline();

                animateScope.start(updateView);
                scope.$on('$destroy', function() { animateScope.stop(); });

                scope.log = function(data){
                    console.log(data);
                };
            }

        };
    }]);

'use strict';

/***
 * TimelineRender is a set of function, which draws timeline based on it's state, described by recordsProvider, scaleManager
 * @param canvas
 * @param recordsProvider
 * @param scaleManager
 * @param timelineConfig
 * @param animationState
 * @param debugEventsMode
 */
function TimelineCanvasRender(canvas, timelineConfig, recordsProvider, scaleManager, animationState, debugSettings){

    this.canvas = canvas;
    this.recordsProvider = recordsProvider;
    this.scaleManager = scaleManager;
    this.animationState = animationState;
    this.debugEventsMode = debugSettings.debugEventsMode;
    this.allowDebug = debugSettings.allowDebug;

    var self = this;

    function blurColor(color,alpha){ // Bluring function [r,g,b] + alpha -> String

        if(typeof(color)=='string'){ // do not try to change strings
            return color;
        }
        if(typeof(alpha) == 'undefined'){
            alpha = 1;
        }
        if(alpha > 1) { // Not so good. It's hack. Somewhere
            console.error('bad value', alpha);
        }

        if(color.length > 3){ //Array already has alpha channel
            alpha = alpha * color[3];
        }

        return 'rgba(' +
            Math.round(color[0]) + ',' +
            Math.round(color[1]) + ',' +
            Math.round(color[2]) + ',' +
            alpha + ')';
    }
    function formatFont(font){
        return font.weight + ' ' + font.size.toFixed(4) + 'px ' + font.face;
    }

    function clearTimeline(){
        var context = canvas.getContext('2d');
        context.fillStyle = blurColor(timelineConfig.timelineBgColor,1);
        context.clearRect(0, 0, self.canvas.width, self.canvas.height);
        context.lineWidth = timelineConfig.lineWidth;
        return context;
    }

    // !!! Labels logic
    function drawTopLabels(context){
        drawLabelsRow(context,
            self.scaleManager.levels.top.index,
            self.scaleManager.levels.top.index,
            1,
            'topFormat',
            timelineConfig.topLabelFixed,
            0,
            timelineConfig.topLabelHeight,
            timelineConfig.topLabelFont,
            timelineConfig.topLabelAlign,
            timelineConfig.topLabelBgColor,
            timelineConfig.topLabelBgOddColor,
            timelineConfig.topLabelMarkerColor,
            timelineConfig.topLabelPositionFix,
            timelineConfig.topLabelMarkerAttach,
            timelineConfig.topLabelMarkerHeight);
    }

    function drawLabels(context){
        if(timelineConfig.oldStyle) {
            drawLabelsOldStyle(
                context,
                'format',
                timelineConfig.labelFixed,
                timelineConfig.topLabelHeight,
                timelineConfig.labelHeight,
                timelineConfig.labelFont,
                timelineConfig.labelAlign,
                timelineConfig.labelBgColor,
                timelineConfig.labelBgColor,
                timelineConfig.labelMarkerColor,
                timelineConfig.labelMarkerAttach,
                timelineConfig.levelsSettings);

            return;
        }

        drawLabelsRow(
            context,
            animationState.currentLevels.labels.index,
            animationState.targetLevels.labels.index,
            self.animationState.zooming,
            'format',
            timelineConfig.labelFixed,
            timelineConfig.topLabelHeight,
            timelineConfig.labelHeight,
            timelineConfig.labelFont,
            timelineConfig.labelAlign,
            timelineConfig.labelBgColor,
            timelineConfig.labelBgColor,
            timelineConfig.labelMarkerColor,
            timelineConfig.labelPositionFix,
            timelineConfig.labelMarkerAttach,
            timelineConfig.labelMarkerHeight);

        drawMarks(context);
    }

    function drawMarks(context){
        drawLabelsRow(
            context,
            animationState.currentLevels.marks.index,
            animationState.targetLevels.marks.index,
            self.animationState.zooming,
            false,// No format
            'none',
            timelineConfig.topLabelHeight,
            timelineConfig.labelHeight,
            null,//font
            null,//align
            null,//no bg label
            null,//no bg label
            timelineConfig.marksColor,
            0,
            timelineConfig.marksAttach,
            timelineConfig.marksHeight );
    }


    function drawLabelsOldStyle(context,
                                labelFormat, labelFixed, levelTop, levelHeight,
                                font, labelAlign, bgColor, bgOddColor, markColor, markAttach, levelsSettings){
        // 1. we need four levels:
        // Mark only
        // small label
        // middle label
        // Large label

        //2. Every level can be animated
        //3. Animation includes changing of height and font-size and transparency for colors

        function getPointAnimation(pointLevelIndex, levelName){ // Get transition value for animation for this level
            var animation = self.animationState.zooming;

            var minLevel = getLevelMinValue(levelName);

            if( pointLevelIndex <= minLevel ) {
                return null; // Do not animate
            }

            if(animationState.targetLevels[levelName].index == animationState.currentLevels[levelName].index) { // Do not animate
                return null;
            }

            if(animationState.targetLevels[levelName].index > animationState.currentLevels[levelName].index){ // Increasing
                return 1 - animation;
            }

            return animation;
        }

        function getLevelMinValue(levelName){ // Get lower levelinex for this level
            return Math.min(animationState.currentLevels[levelName].index,animationState.targetLevels[levelName].index);
        }

        function getLevelMaxValue(levelName){
            return Math.max(animationState.currentLevels[levelName].index,animationState.targetLevels[levelName].index);
        }

        function getBestLevelName(pointLevelIndex){ // Find best level for this levelindex
            if(pointLevelIndex <= getLevelMaxValue('labels'))
                return 'labels';

            if(pointLevelIndex <= getLevelMaxValue('middle'))
                return 'middle';

            if(pointLevelIndex <= getLevelMaxValue('small'))
                return 'small';

            if(pointLevelIndex <= getLevelMaxValue('marks'))
                return 'marks';

            return 'events';
        }

        function getPrevLevelName(pointLevelIndex){
            if(pointLevelIndex <= getLevelMinValue('labels'))
                return 'labels';

            if(pointLevelIndex <= getLevelMinValue('middle'))
                return 'middle';

            if(pointLevelIndex <= getLevelMinValue('small'))
                return 'small';

            if(pointLevelIndex <= getLevelMinValue('marks'))
                return 'marks';

            return 'events';
        }

        function interpolate(alpha, min, max){ // Find value during animation
            return min + alpha * (max - min);
        }


        var levelIndex = Math.max(self.scaleManager.levels.marks.index, animationState.targetLevels.marks.index); // Target level is lowest of visible

        // If it this point levelIndex is invisible (less than one pixel per mark) - skip it!
        var level = RulerModel.levels[levelIndex]; // Actual calculating level
        var levelDetailizaion = level.interval.getMilliseconds();
        while( levelDetailizaion < timelineConfig.minMarkWidth * self.scaleManager.msPerPixel)
        {
            levelIndex --;
            level = RulerModel.levels[levelIndex];
            levelDetailizaion = level.interval.getMilliseconds();
        }

        var start = self.scaleManager.alignStart(RulerModel.levels[levelIndex>0?(levelIndex-1):0]); // Align start by upper level!
        var point = start;
        var end = self.scaleManager.alignEnd(level);

        var counter = 0;// Protection from neverending cycles.

        while(point <= end && counter++ < 3000){
            var odd = bgColor!= bgOddColor || Math.round((point.getTime() / levelDetailizaion)) % 2 === 1; // add or even for zebra coloring

            var pointLevelIndex = RulerModel.findBestLevelIndex(point, levelIndex);

            var levelName = getBestLevelName(pointLevelIndex); // Here we detect best level for this particular point

            var animation = getPointAnimation(pointLevelIndex, levelName); // We detect, if level is changing
            var markSize = levelsSettings[levelName].markSize;
            var transparency = levelsSettings[levelName].transparency;
            var fontSize = levelsSettings[levelName].fontSize;
            var labelPositionFix = levelsSettings[levelName].labelPositionFix;

            if(animation !== null){ //scaling between upperLevel and levelName to up
                var animationLevelName = getPrevLevelName(pointLevelIndex);
                markSize = interpolate(animation, markSize, levelsSettings[animationLevelName].markSize);
                transparency = interpolate(animation, transparency, levelsSettings[animationLevelName].transparency);
                fontSize = interpolate(animation, fontSize, levelsSettings[animationLevelName].fontSize);
                labelPositionFix = interpolate(animation, labelPositionFix, levelsSettings[animationLevelName].labelPositionFix);
            }

            font.size = fontSize; // Set font size for label

            //Draw lebel, using calculated values
            drawLabel(context, point, level, transparency,
                labelFormat, labelFixed, levelTop, levelHeight,
                font, labelAlign, odd?bgColor: bgOddColor, markColor, labelPositionFix, markAttach, markSize);

            point = level.interval.addToDate(point);
        }
        if(counter == 3000){
            console.error('counter problem!', start,point, end, level);
        }
    }

    function drawLabelsRow (context, currentLevelIndex, taretLevelIndex, animation,
                            labelFormat, labelFixed, levelTop, levelHeight,
                            font, labelAlign, bgColor, bgOddColor, markColor, labelPositionFix, markAttach, markHeight){

        var levelIndex = Math.max(currentLevelIndex, taretLevelIndex);
        var level = RulerModel.levels[levelIndex]; // Actual calculating level

        var start1 = self.scaleManager.alignStart(level);
        var start = self.scaleManager.alignStart(level);
        var end = self.scaleManager.alignEnd(level);
        var levelDetailizaion = level.interval.getMilliseconds();


        var counter = 1000;
        while(start <= end && counter-- > 0){
            var odd = Math.round((start.getTime() / levelDetailizaion)) % 2 === 1;
            var pointLevelIndex = RulerModel.findBestLevelIndex(start);
            var alpha = 1;
            if(pointLevelIndex > taretLevelIndex){ //fade out
                alpha = 1 - animation;
            }else if(pointLevelIndex > currentLevelIndex){ // fade in
                alpha =  animation;
            }
            drawLabel(context,start,level,alpha,
                labelFormat, labelFixed, levelTop, levelHeight, font, labelAlign, odd?bgColor: bgOddColor, markColor, labelPositionFix, markAttach, markHeight);
            start = level.interval.addToDate(start);
        }
        if(counter < 1){
            console.error('problem!', start1, start, end, level);
        }
    }
    function drawLabel( context, date, level, alpha,
                        labelFormat, labelFixed, levelTop, levelHeight, font, labelAlign, bgColor, markColor, labelPositionFix, markAttach, markHeight){

        var coordinate = self.scaleManager.dateToScreenCoordinate(date);

        if(labelFormat) {
            context.font = formatFont(font);

            var nextDate = level.interval.addToDate(date);
            var stopCoordinate = self.scaleManager.dateToScreenCoordinate(nextDate);
            //var nextLevel = labelFixed?level:RulerModel.findBestLevel(nextDate);
            //var nextLabel = dateFormat(new Date(nextDate), nextLevel[labelFormat]);

            var bestLevel = labelFixed?level:RulerModel.findBestLevel(date);
            var label = dateFormat(new Date(date), bestLevel[labelFormat]);
            var textWidth = context.measureText(label).width;

            var textStart = levelTop * self.canvas.height // Top border
                + (levelHeight * self.canvas.height - font.size) / 2 // Top padding
                + font.size + labelPositionFix; // Font size

            var x = 0;
            switch (labelAlign) {
                case 'center':
                    var leftBound = Math.max(0, coordinate);
                    var rightBound = Math.min(stopCoordinate, self.canvas.width);
                    var center = (leftBound + rightBound) / 2;

                    x = self.scaleManager.bound(
                            coordinate + timelineConfig.labelPadding,
                            center - textWidth / 2,
                            stopCoordinate - textWidth - timelineConfig.labelPadding
                    );

                    break;

                case 'left':
                    x = self.scaleManager.bound(
                        timelineConfig.labelPadding,
                            coordinate + timelineConfig.labelPadding,
                            stopCoordinate - textWidth - timelineConfig.labelPadding
                    );

                    break;

                case 'above':
                default:
                    x = coordinate - textWidth / 2;
                    break;
            }
        }


        var markTop = markAttach == 'top' ? levelTop : levelTop + levelHeight - markHeight ;

        if(bgColor) {
            context.fillStyle = blurColor(bgColor, alpha);

            switch (labelAlign) {
                case 'center': // fill the whole interval
                case 'left':
                    context.fillRect(coordinate, levelTop * self.canvas.height, stopCoordinate - coordinate, levelHeight * self.canvas.height); // above
                    break;
            }
        }

        if(markColor) {
            context.strokeStyle = blurColor(markColor, alpha);
            context.beginPath();
            context.moveTo(coordinate + 0.5, markTop * self.canvas.height);
            context.lineTo(coordinate + 0.5, (markTop + markHeight) * self.canvas.height);
            context.stroke();
        }

        if(bgColor) {
            context.fillStyle = blurColor(bgColor, alpha);
            switch (labelAlign) {
                case 'center': // fill the whole interval
                case 'left':
                    break;
                case 'above':
                default:
                    context.clearRect(x, textStart - font.size, textWidth, font.size); // above
                    break;
            }
        }

        if(labelFormat) {
            context.fillStyle = blurColor(font.color, alpha);
            context.fillText(label, x, textStart);
        }
    }

    function debugEvents(context){
        var top = (timelineConfig.topLabelHeight + timelineConfig.labelHeight) * self.canvas.height; // Top border

        context.fillStyle = blurColor(timelineConfig.chunksBgColor,1);

        context.fillRect(0, top, self.canvas.width, timelineConfig.chunkHeight * self.canvas.height);

        if(self.recordsProvider && self.recordsProvider.chunksTree) {
            var targetLevelIndex = self.scaleManager.levels.events.index;

            for(var levelIndex=0;levelIndex<RulerModel.levels.length;levelIndex++) {
                var level = RulerModel.levels[levelIndex];
                var start = self.scaleManager.alignStart(level);
                var end = self.scaleManager.alignEnd(level);

                // 1. Splice events
                var events = self.recordsProvider.getIntervalRecords(start, end, levelIndex, levelIndex != targetLevelIndex);

                // 2. Draw em!
                for (var i = 0; i < events.length; i++) {
                    drawEvent(context, events[i], levelIndex, true, targetLevelIndex);
                }
            }
        }
    }
    // !!! Draw events
    function drawOrCheckEvents(context, mouseX, mouseY){
        var top = (timelineConfig.topLabelHeight + timelineConfig.labelHeight) * self.canvas.height; // Top border

        if(context ) {
            context.fillStyle = blurColor(timelineConfig.chunksBgColor, 1);

            context.fillRect(0, top, self.canvas.width, timelineConfig.chunkHeight * self.canvas.height);

            var level = self.scaleManager.levels.events.level;
            var levelIndex = self.scaleManager.levels.events.index;
            var start = self.scaleManager.alignStart(level);
            var end = self.scaleManager.alignEnd(level);


            if (self.recordsProvider && self.recordsProvider.chunksTree) {
                // 1. Splice events
                var events = self.recordsProvider.getIntervalRecords(start, end, levelIndex);



                // 2. Draw em!
                for (var i = 0; i < events.length; i++) {
                    drawEvent(context, events[i], levelIndex);
                }
            }
        }
        return mouseX && mouseY > top && (mouseY < top + timelineConfig.chunkHeight * self.canvas.height);
    }
    function drawEvent(context,chunk, levelIndex, debug, targetLevelIndex){
        var startCoordinate = self.scaleManager.dateToScreenCoordinate(chunk.start);
        var endCoordinate = self.scaleManager.dateToScreenCoordinate(chunk.end);

        var blur = 1;//chunk.level/(RulerModel.levels.length - 1);

        context.fillStyle = blurColor(timelineConfig.exactChunkColor,blur); //green

        var top = (timelineConfig.topLabelHeight + timelineConfig.labelHeight) * self.canvas.height;
        var height = timelineConfig.chunkHeight * self.canvas.height;

        if(debug){
            if(levelIndex == chunk.level) { // not exact chunk
                context.fillStyle = blurColor(timelineConfig.loadingChunkColor,blur); //blue
            }

            if(!chunk.level){ //blind spot!
                context.fillStyle = blurColor(timelineConfig.blindChunkColor,1); // red
            }

            if(targetLevelIndex == levelIndex) {
                context.fillStyle = blurColor(timelineConfig.highlighChunkColor, 1); //yellow
            }

            top += (1+levelIndex) / RulerModel.levels.length * height;
            height /= RulerModel.levels.length;

            top = Math.floor(top);
            height = Math.ceil(height);
        }

        context.fillRect(startCoordinate - timelineConfig.minChunkWidth/2,
            top,
                (endCoordinate - startCoordinate) + timelineConfig.minChunkWidth/2,
            height);
    }


    var lastMinuteTexture = false;
    var lastMinuteTextureImg = false;
    function drawLastMinute(context){
        // 1. Get start coordinate for last minute
        var end = self.scaleManager.end;
        var start = self.scaleManager.lastMinute();


        if(start >= self.scaleManager.visibleEnd){
            return; // Do not draw - just skip it
        }
        // 2. Get texture

        if(lastMinuteTexture){
            context.fillStyle = lastMinuteTexture;
        }else{
            if(!lastMinuteTextureImg) {
                var img = new Image();
                img.onload = function () {
                    lastMinuteTexture = context.createPattern(img, 'repeat');
                    context.fillStyle = lastMinuteTexture;
                };
                img.src = 'images/lastminute.png';
                lastMinuteTextureImg = img;
            }
            context.fillStyle = blurColor(timelineConfig.lastMinuteColor,1);
        }

        // 3. Draw last minute with texture
        var startCoordinate = self.scaleManager.dateToScreenCoordinate(start);
        var endCoordinate = self.scaleManager.dateToScreenCoordinate(end);

        var top = (timelineConfig.topLabelHeight + timelineConfig.labelHeight) * self.canvas.height;
        var height = timelineConfig.chunkHeight * self.canvas.height;

        var offset_x = - (start / timelineConfig.lastMinuteAnimationMs) % timelineConfig.lastMinuteTextureSize;

        context.save();
        context.translate(offset_x, 0);

        context.fillRect(startCoordinate - timelineConfig.minChunkWidth/2 - offset_x,
            top,
                (endCoordinate - startCoordinate) + timelineConfig.minChunkWidth/2,
            height);

        context.restore();
    }

    var scrollBarSliderWidth = 0;
    // !!! Draw ScrollBar
    function drawOrCheckScrollBar(context, mouseX, mouseY, catchScrollBarSlider){
        var top = (timelineConfig.topLabelHeight + timelineConfig.labelHeight + timelineConfig.chunkHeight) * self.canvas.height; // Top border

        //2.
        var relativeCenter =  self.scaleManager.getRelativeCenter();
        var relativeWidth =  self.scaleManager.getRelativeWidth();

        scrollBarSliderWidth = Math.max(self.canvas.width * relativeWidth, timelineConfig.minScrollBarWidth);
        // Correction for width if it has minimum width
        var startCoordinate = self.scaleManager.bound( 0, (self.canvas.width * relativeCenter - scrollBarSliderWidth/2), self.canvas.width - scrollBarSliderWidth) ;

        var mouseInScrollbarRow = mouseY >= top;
        var mouseInScrollbarSlider = mouseX >= startCoordinate && mouseX <= startCoordinate + scrollBarSliderWidth && mouseY >= top;

        if(context) {
            //1. DrawBG
            context.fillStyle = blurColor(timelineConfig.scrollBarBgColor, 1);
            context.fillRect(0, top, self.canvas.width, timelineConfig.scrollBarHeight * self.canvas.height);

            //2. drawOrCheckScrollBar
            context.fillStyle = (mouseInScrollbarSlider || catchScrollBarSlider) ? blurColor(timelineConfig.scrollBarHighlightColor, 1) : blurColor(timelineConfig.scrollBarColor, 1);
            context.fillRect(startCoordinate, top, scrollBarSliderWidth, timelineConfig.scrollBarHeight * self.canvas.height);
        }else{

            if(mouseInScrollbarSlider || catchScrollBarSlider){
                mouseInScrollbarSlider = mouseX - startCoordinate;
            }

            if(mouseInScrollbarRow){
                mouseInScrollbarRow = mouseX - startCoordinate;
            }

            return {
                scrollbar: mouseInScrollbarRow,
                scrollbarSlider: mouseInScrollbarSlider
            }
        }
    }

    // !!! Draw and position for timeMarker
    function drawTimeMarker(context){
        if(!self.scaleManager.playedPosition || self.scaleManager.end - self.scaleManager.playedPosition < self.scaleManager.stickToLiveMs){
            return;
        }

        drawMarker(context, self.scaleManager.playedPosition, timelineConfig.timeMarkerColor, timelineConfig.timeMarkerTextColor)
    }

    function drawPointerMarker(context, mouseX){

        if(window.jscd.mobile || !mouseX){
            return;
        }

        drawMarker(context, self.scaleManager.screenCoordinateToDate(mouseX),timelineConfig.pointerMarkerColor,timelineConfig.pointerMarkerTextColor);
    }

    function drawMarker(context, date, markerColor, textColor){
        var coordinate =  self.scaleManager.dateToScreenCoordinate(date);

        if(coordinate < 0 || coordinate > self.canvas.width ) {
            return;
        }

        date = new Date(date);

        var height = timelineConfig.markerHeight * self.canvas.height;

        // Line
        context.lineWidth = timelineConfig.timeMarkerLineWidth;
        context.strokeStyle = blurColor(markerColor,1);
        context.fillStyle = blurColor(markerColor,1);

        var top = (timelineConfig.topLabelHeight + timelineConfig.labelHeight) * self.canvas.height;

        context.beginPath();
        context.moveTo(0.5 + coordinate, top);
        context.lineTo(0.5 + coordinate, Math.round(self.canvas.height - timelineConfig.scrollBarHeight * self.canvas.height));
        context.stroke();

        var startCoord = coordinate - timelineConfig.markerWidth /2;
        if(startCoord < 0){
            startCoord = 0;
        }
        if(startCoord > self.canvas.width - timelineConfig.markerWidth){
            startCoord = self.canvas.width - timelineConfig.markerWidth;
        }

        // Bubble
        context.fillRect(startCoord, 0, timelineConfig.markerWidth, height );

        // Triangle
        context.beginPath();
        context.moveTo(0.5 + coordinate + timelineConfig.markerTriangleHeight * self.canvas.height, height);
        context.lineTo(0.5 + coordinate, height + timelineConfig.markerTriangleHeight * self.canvas.height);
        context.lineTo(0.5 + coordinate - timelineConfig.markerTriangleHeight * self.canvas.height, height);
        context.closePath();
        context.fill();

        // Labels
        context.fillStyle = blurColor(textColor,1);
        context.font = formatFont(timelineConfig.markerDateFont);
        coordinate = startCoord + timelineConfig.markerWidth /2; // Set actual center of the marker

        var dateString = dateFormat(date, timelineConfig.dateFormat);
        var dateWidth = context.measureText(dateString).width;
        var textStart = (height - timelineConfig.markerDateFont.size) / 2;
        context.fillText(dateString,coordinate - dateWidth/2, textStart);


        context.font = formatFont(timelineConfig.markerTimeFont);
        dateString = dateFormat(date, timelineConfig.timeFormat);
        dateWidth = context.measureText(dateString).width;
        textStart = height/2 + textStart;
        context.fillText(dateString,coordinate - dateWidth/2, textStart);

    }




    var scrollLeftEnablingTimer = null;
    var ableToScrollLeft = false;

    var scrollRightEnablingTimer = null;
    var ableToScrollRight = false;


    function drawOrCheckScrollButtons(context, mouseX, mouseY){

        var canScrollRight = self.scaleManager.canScroll(false);
        var mouseNearRightBorder = mouseX > self.canvas.width - timelineConfig.scrollButtonsWidth && mouseX < self.canvas.width;
        var mouseOverRightScrollButton = canScrollRight && mouseNearRightBorder;

        var canScrollLeft = self.scaleManager.canScroll(true);
        var mouseNearLeftBorder = mouseX < timelineConfig.scrollButtonsWidth && mouseX > 0;
        var mouseOverLeftScrollButton = canScrollLeft && mouseNearLeftBorder;

        if(context) {
            if (canScrollLeft) {
                if(ableToScrollLeft || true) {
                    drawScrollButton(context, true, mouseOverLeftScrollButton);
                }else{
                    if(!scrollLeftEnablingTimer) {
                        scrollLeftEnablingTimer = setTimeout(function () {
                            scrollLeftEnablingTimer = null;
                            //ableToScrollLeft = true;
                        }, timelineConfig.animationDuration);
                    }
                }
            }else{
                ableToScrollLeft = false;
                if(scrollLeftEnablingTimer){
                    clearTimeout(scrollLeftEnablingTimer);
                    scrollLeftEnablingTimer = null;
                }
            }

            if (canScrollRight) {
                if(ableToScrollRight || true) {
                    drawScrollButton(context, false, mouseOverRightScrollButton);
                }else{
                    if(!scrollRightEnablingTimer) {
                        scrollRightEnablingTimer = setTimeout(function () {
                            scrollRightEnablingTimer = null;
                            //ableToScrollRight = true;
                        }, timelineConfig.animationDuration);
                    }
                }
            }else{
                ableToScrollRight = false;
                if(scrollRightEnablingTimer){
                    clearTimeout(scrollRightEnablingTimer);
                }
                scrollRightEnablingTimer = null;
            }
        }
        return {
            leftButton : mouseOverLeftScrollButton,
            rightButton: mouseOverRightScrollButton,
            rightBorder: mouseNearRightBorder,
            leftBorder: mouseNearLeftBorder
        }
    }

    function drawScrollButton(context, left, active){

        context.fillStyle = active? blurColor(timelineConfig.scrollButtonsActiveColor,1):blurColor(timelineConfig.scrollButtonsColor,1);

        var startCoordinate = left ? 0: self.canvas.width - timelineConfig.scrollButtonsWidth;
        var height = timelineConfig.scrollButtonsHeight * self.canvas.height;

        context.fillRect(startCoordinate, self.canvas.height - height, timelineConfig.scrollButtonsWidth, height );

        context.lineWidth = timelineConfig.scrollButtonsArrowLineWidth;
        context.strokeStyle =  active? blurColor(timelineConfig.scrollButtonsArrowActiveColor,1):blurColor(timelineConfig.scrollButtonsArrowColor,1);

        var leftCoordinate = startCoordinate + ( timelineConfig.scrollButtonsWidth - timelineConfig.scrollButtonsArrow.width)/2;
        var rightCoordinate = leftCoordinate + timelineConfig.scrollButtonsArrow.width;

        var topCoordinate = self.canvas.height - (height + timelineConfig.scrollButtonsArrow.size) / 2;
        var bottomCoordinate = topCoordinate + timelineConfig.scrollButtonsArrow.size;


        context.beginPath();
        context.moveTo(left?rightCoordinate:leftCoordinate, topCoordinate);
        context.lineTo(left?leftCoordinate:rightCoordinate, (topCoordinate + bottomCoordinate)/2);
        context.lineTo(left?rightCoordinate:leftCoordinate, bottomCoordinate);
        context.stroke();

    }

    function FpsCalculator(){
        var lastLoop = (new Date()).getMilliseconds();
        var count = 1;
        var fps = 0;

        return function () {
            var currentLoop = (new Date()).getMilliseconds();
            if (lastLoop > currentLoop) {
                fps = count;
                count = 1;
            } else {
                count += 1;
            }
            lastLoop = currentLoop;
            return fps;
        };
    }
    var getFps = null;
    function calcAndDrawFPS(context, mouseX, mouseY){
        if(!getFps){
            getFps = FpsCalculator();
        }
        var fps = "fps: " + getFps();// + ' (' + mouseX + ',' + mouseY + ') ' + self.scaleManager.dateToScreenCoordinate(self.scaleManager.screenCoordinateToDate(mouseX));

        context.font = formatFont(timelineConfig.markerDateFont);
        context.fillStyle = blurColor(timelineConfig.pointerMarkerTextColor, 0.7);
        context.fillText(fps, 5, timelineConfig.markerDateFont.size);

    }
    this.setRecordsProvider = function(recordsProvider){
        this.recordsProvider = recordsProvider;
    };
    this.Draw = function(mouseX, mouseY, catchScrollBarSlider){

        var context = clearTimeline();

        drawTopLabels(context);

        drawLabels(context);

        if(debugSettings.allowDebug){
            calcAndDrawFPS(context, mouseX, mouseY);
        }


        var mouseOverEvents = false;
        if(!self.debugEventsMode) {
            mouseOverEvents = drawOrCheckEvents(context,mouseX, mouseY);
        }else{
            mouseOverEvents = drawOrCheckEvents(null, mouseX, mouseY);
            debugEvents(context);
        }

        drawLastMinute(context);

        drawOrCheckScrollBar(context, mouseX, mouseY, catchScrollBarSlider);

        var buttonsState = drawOrCheckScrollButtons(context, mouseX, mouseY);


        drawTimeMarker(context);

        if(mouseOverEvents && !buttonsState.rightButton && ! buttonsState.leftButton) {
            drawPointerMarker(context, mouseX, mouseY);
        }
    };

    this.checkElementsUnderCursor = function(mouseX, mouseY){
        var result = {
            scrollbar: false,
            scrollbarSlider: false,
            leftButton: false,
            rightButton: false,
            eventsRow: false
        };

        var buttons = drawOrCheckScrollButtons(null, mouseX, mouseY);
        _.extend(result, buttons);
        if(result.leftButton || result.rightButton)
        {
            return result;
        }

        var scrollbar = drawOrCheckScrollBar(null, mouseX, mouseY);
        _.extend(result, scrollbar);
        if(result.scrollbar){
            return result;
        }

        result.eventsRow = drawOrCheckEvents(null, mouseX, mouseY);

        result.timeline = mouseY > 0 && mouseX > 0 && mouseX < canvas.width && mouseY < canvas.height;

        return result;
    };
}

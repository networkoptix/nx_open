'use strict';

function TimelineActions(timelineConfig, positionProvider, scaleManager, animationState, animateScope, scope ) {
    this.animateScope = animateScope;
    this.scope = scope;
    this.positionProvider = positionProvider;
    this.scaleManager = scaleManager;
    this.animationState = animationState;
    this.timelineConfig = timelineConfig;

    this.stopDelay = null;
    this.nextPlayedPosition = false;
    this.scope.lastPlayedPosition = 0;

    this.scrollingNow = false;
    this.scrollingLeft = false;
    this.scrollingSpeed = 0;

    this.zoomingNow = false;
    this.zoomingIn = false;

    this.zoomByWheelTarget = 0;

    this.catchScrollBar = false;

    this.catchTimeline = false;
}
TimelineActions.prototype.setPositionProvider = function (positionProvider){
    this.positionProvider = positionProvider;
};

TimelineActions.prototype.goToLive = function(){
    var self = this;
    var moveDate = self.scaleManager.screenCoordinateToDate(1);
    self.animateScope.progress(self.scope, 'goingToLive' ).then(
        function(){
            var activeDate = (new Date()).getTime();
            self.scaleManager.setAnchorDateAndPoint(activeDate,1);
            self.scaleManager.watchPlaying(activeDate, true);
        },
        function(){},
        function(val){
            var activeDate = moveDate + val * ((new Date()).getTime() - moveDate);
            self.scaleManager.setAnchorDateAndPoint(activeDate,1);
        });
};

TimelineActions.prototype.playPause = function() {
    if (!this.positionProvider.playing) {
        this.scaleManager.watchPlaying();
    }
};


TimelineActions.prototype.delayWatchingPlayingPosition = function(){
    var self = this;
    if(!self.stopDelay) {
        self.scaleManager.stopWatching();
    }else{
        clearTimeout(self.stopDelay);
    }
    self.stopDelay = setTimeout(function(){
        self.scaleManager.releaseWatching();
        self.stopDelay = null;
    },self.timelineConfig.animationDuration);
};


TimelineActions.prototype.stopAnimatingMove = function(){
    var animation = this.animateScope.animating(this.scope, 'lastPlayedPosition');
    if(animation){
        animation.breakAnimation();
    }
};
TimelineActions.prototype.updatePosition = function(){
    var self = this;
    if(self.positionProvider) {
        if(self.nextPlayedPosition){
            if(self.positionProvider.liveMode || self.nextPlayedPosition == self.positionProvider.playedPosition){
                self.nextPlayedPosition = false;
            }
            // Update live position anyways
            self.scaleManager.tryToSetLiveDate(null, self.positionProvider.liveMode);
            return; // ignore changes until played position wasn't changed
        }

        var intervalMs = Math.abs(self.scope.lastPlayedPosition - self.positionProvider.playedPosition);
        var duration = Math.min(self.timelineConfig.animationDuration, intervalMs);
        var largeJump = intervalMs > 2 * self.timelineConfig.animationDuration;

        if(intervalMs == 0){
            // Update live position anyways
            self.scaleManager.tryToSetLiveDate(null, self.positionProvider.liveMode);
            return;
        }
        if (!largeJump || !self.animateScope.animating(self.scope, 'lastPlayedPosition')) { // Large jump
            // this animation makes scaleManager move smoothly while playing
            self.animateScope.animate(self.scope, 'lastPlayedPosition', self.positionProvider.playedPosition,
                largeJump ? 'smooth' : 'linear', duration).then(
                function () {},
                function () {},
                function (value) {
                    if(self.nextPlayedPosition){
                        console.log("problem with playing position",value,self.nextPlayedPosition);
                        self.scope.lastPlayedPosition = self.nextPlayedPosition;
                        return;
                    }
                    self.scaleManager.tryToSetLiveDate(value, self.positionProvider.liveMode);
                });
        }
    }
};

TimelineActions.prototype.setAnchorCoordinate = function(mouseX){
    var position = this.scaleManager.setAnchorCoordinate(mouseX); // Set position to keep and get time to set

    this.nextPlayedPosition = position; // Setting this we will ignore timeupdates until new position starts playing

    this.stopAnimatingMove(); // Instantly jump to new position
    this.scope.lastPlayedPosition = position;
    this.scaleManager.tryToSetLiveDate(position, this.positionProvider.liveMode);

    return position;
};



// linear is for holding function
TimelineActions.prototype.animateScroll = function(targetPosition, linear){
    var self = this;
    self.delayWatchingPlayingPosition();
    self.scope.scrollTarget = self.scaleManager.scroll();
    self.animateScope.animate(self.scope, 'scrollTarget', targetPosition, linear?'linear':false).
        then(
        function(){
            self.scrollingNow = false;
        },
        function(){},
        function(value){
            self.scaleManager.scroll(value);
        }
    );
};

// Constant scrolling process
TimelineActions.prototype.scrollingRenew = function(stoping){
    if(this.scrollingNow) {
        var moveScroll = (this.scrollingLeft ? -1 : 1) * this.scrollingSpeed;
        var scrollTarget = this.scaleManager.getScrollByPixelsTarget(moveScroll);
        this.animateScroll(scrollTarget,stoping);
    }
};
TimelineActions.prototype.scrollingStart = function(left,speed){
    this.scrollingLeft = left;
    this.scrollingNow = true;
    this.scrollingSpeed = speed;
    this.scrollingRenew ();
};
TimelineActions.prototype.scrollingStop = function(){
    if(this.scrollingNow) {
        this.scrollingRenew(true); // Smooth slowdown for scrolling
        this.scrollingNow = false;
    }
};


TimelineActions.prototype.scrollByPixels = function(pixels){
    this.scaleManager.scrollByPixels(pixels);
    this.delayWatchingPlayingPosition();   
};


/* Zoom logic */

// Main zoom function - handle zoom logic, animations, etc
// gets absolute zoom value - to target level from 0 to 1
TimelineActions.prototype.zoomTo = function(zoomTarget, zoomCoordinate, instant){
    var self = this;
    zoomTarget = this.scaleManager.boundZoom(zoomTarget);

    if(typeof(zoomCoordinate)=='undefined'){
        zoomCoordinate = this.scaleManager.viewportWidth/2;
    }

    var zoom = self.scaleManager.zoom();
    if(zoom == zoomTarget){
        return; // No need to zoom
    }

    this.updateZoomLevels(zoomTarget);
    function setZoom(value){

    // Actually, we'd better use zoom around coordinate instead of date?
        self.scaleManager.zoomAroundPoint(
            value,
            zoomCoordinate
        );
        self.scaleManager.checkZoomAsync().then(function(){
            // TODO: here we need to apply the change to scope - call digest if anything changed
            self.scope.$digest();
        });
        self.delayWatchingPlayingPosition();
    }


    if(!instant) {
        if(!self.scope.zoomTarget) {
            self.scope.zoomTarget = self.scaleManager.zoom();
        }

        self.delayWatchingPlayingPosition();
        self.animateScope.animate(self.scope, 'zoomTarget', zoomTarget, 'dryResistance').then(
            function () {
                self.zoomByWheelTarget = 0; // When animation if over - clean zoomByWheelTarget
            },
            function () {},
            setZoom);
    }else{
        setZoom(zoomTarget);
        self.scope.zoomTarget = self.scaleManager.zoom();
        self.zoomByWheelTarget = 0;
    }
};

// Update zoom levels - run animation if needed
TimelineActions.prototype.updateZoomLevels = function(zoomTarget){
    var self = this;
    function levelsChanged(newLevels,oldLevels){
        var changedLevels = [];
        if(newLevels && (!oldLevels || !oldLevels.labels)){
            return ['labels', 'middle', 'small', 'marks'];
        }

        if(newLevels.labels.index != oldLevels.labels.index) {
            changedLevels.push('labels');
        }

        if(newLevels.middle.index != oldLevels.middle.index) {
            changedLevels.push('middle');
        }

        if(newLevels.small.index != oldLevels.small.index) {
            changedLevels.push('small');
        }

        if(newLevels.marks.index != oldLevels.marks.index) {
            changedLevels.push('marks');
        }

        return changedLevels;
    }

    //Find final levels for this zoom and run animation:
    var newTargetLevels = self.scaleManager.targetLevels(zoomTarget);
    var changedLevels = levelsChanged(newTargetLevels, self.animationState.targetLevels);
    if(changedLevels.length>0){
        self.animationState.targetLevels = newTargetLevels;
    }
    for(var i in changedLevels){
        var changedLevel = changedLevels[i];

        self.animationState[changedLevel] = 0; // Start animation over
        self.animationState.currentLevels[changedLevel] = self.scaleManager.levels[changedLevel]; // force old animation to complete
        (function(changedLevel){
            self.animateScope.progress(self.scope, 'zooming' + changedLevel, 'dryResistance').then(function(){
                self.animationState.currentLevels[changedLevel] = self.scaleManager.levels[changedLevel];
            },function(){
                // ignore animation re-run
            },function(value){
                self.animationState[changedLevel] = value;
            });
        })(changedLevel);
    }

};

// Relative zoom (incremental)
TimelineActions.prototype.zoom = function(zoomIn){
    var markerDate = this.scaleManager.playedPosition;

    //By default - use point under time marker
    var zoomCoordinate = this.scaleManager.dateToScreenCoordinate(markerDate);

    // If time marker is not displayed - use center of the timeline
    if(zoomCoordinate < -1 || zoomCoordinate > this.scaleManager.viewportWidth+1) {
        zoomCoordinate = this.scaleManager.viewportWidth/2;
    }
    // If time marker is displayed in extreme 64px (on both sides) - the extreme side point of the timeline
    if(zoomCoordinate < this.timelineConfig.borderAreaWidth){
        zoomCoordinate = 0;
    }
    if(zoomCoordinate > this.scaleManager.viewportWidth - this.timelineConfig.borderAreaWidth){
        zoomCoordinate = this.scaleManager.viewportWidth;
    }

    var zoomTarget = this.scaleManager.zoom() - (zoomIn ? 1 : -1) * this.timelineConfig.slowZoomSpeed;
    this.zoomTo(zoomTarget, zoomCoordinate, false);
};

// Continuus zooming - every render
TimelineActions.prototype.zoomingRenew = function(){ // renew zooming
    if(this.zoomingNow) { // If continuous zooming is on
        this.zoom(this.zoomingIn); // Repeat incremental zoom
    }
};

// Release zoom button - stop continuus zooming
TimelineActions.prototype.zoomingStop = function() {
    this.zoomingNow = false; //  Stop continuous zooming
    this.zoomByWheelTarget = 0;
    var animation = this.animateScope.animating(self.scope, 'zoomTarget');
    if(animation){
        animation.breakAnimation();
    }
};

// Press zoom button - start continuus zooming
TimelineActions.prototype.zoomingStart = function(zoomIn) {
    // check if we can start zooming in that direction:
    if(this.scaleManager.disableZoomOut&&!zoomIn || this.scaleManager.disableZoomIn&&zoomIn){
        return;
    }
    this.zoomingNow = true; // Start continuous zooming
    this.zoomingIn = zoomIn; // Set continuous zooming direction
    this.zoom(this.zoomingIn); // Run zoom at least once
};

// Double click zoom out button - zoom out completely
TimelineActions.prototype.fullZoomOut = function(){
    this.zoomingStop(); // stop slow zooming
    this.zoomTo(1); // zoom out completely
};

// Zoom by wheel logic - slighly different for Mac and Win (Mac does smooth scroll on OS level, Win - does not
TimelineActions.prototype.zoomByWheel = function(clicks, mouseOverElements, mouseXOverTimeline){
    var zoom = this.scaleManager.zoom(); // Get instant zoom level
    if(window.jscd.touch) { // Mac support - touchpad, clicks changes smoothly
        this.zoomingStop(); // Stop all previous zoom animations
        this.zoomByWheelTarget = zoom - clicks / this.timelineConfig.maxVerticalScrollForZoomWithTouch;
    }else{
        // We need to smooth zoom here
        // Collect zoom changing in zoomTarget
        if(!this.zoomByWheelTarget) {
            this.zoomByWheelTarget = zoom;
        }
        this.zoomByWheelTarget -= clicks / this.timelineConfig.maxVerticalScrollForZoom;
        this.zoomByWheelTarget = this.scaleManager.boundZoom(this.zoomByWheelTarget);
    }

    // Actually, we'd better use zoom coordinate instead of date?

    // Get anchor date for zoom
    var zoomCoordinate = mouseXOverTimeline;
    if(mouseOverElements.rightBorder){ // Close to right border - use right visible end date
        zoomCoordinate = this.scaleManager.viewportWidth;
    }
    if(mouseOverElements.leftBorder){ // Close to left border - use left visible end date
        zoomCoordinate = 0;
    }
    this.zoomTo(this.zoomByWheelTarget, zoomCoordinate, window.jscd.touch);
};

/* / End of Zoom logic */


// Update timeline state - process animations every render
TimelineActions.prototype.updateState = function(){
    this.updatePosition();
    this.zoomingRenew();
    this.scrollingRenew();
};

TimelineActions.prototype.scrollbarSliderDragStart = function(mouseX){
    this.scaleManager.stopWatching();
    this.catchScrollBar = mouseX;
    this.catchScrollSlider = this.scaleManager.scrollSlider();
};
TimelineActions.prototype.scrollbarSliderDrag = function(mouseX){
    if(!this.catchScrollSlider || !this.catchScrollSlider.scrollingWidth){
        // Do not scroll if there is no caught position and on full zoom out
        return;
    }
    if(this.catchScrollBar) {
        var moveScroll = mouseX - this.catchScrollBar;
        var targetScroll = (this.catchScrollSlider.start + moveScroll) / this.catchScrollSlider.scrollingWidth;
        this.scaleManager.scroll(targetScroll);
        return moveScroll !== 0;
    }
    return false;
};

TimelineActions.prototype.scrollbarSliderDragEnd = function(){
    if(this.catchScrollBar) {
        this.scaleManager.releaseWatching();
        this.catchScrollBar = false;
    }
};

TimelineActions.prototype.timelineDragStart = function(mouseX){
    this.scaleManager.stopWatching();
    this.catchTimeline = mouseX;
};
TimelineActions.prototype.timelineDrag = function(mouseX){
    if(this.catchTimeline) {
        var moveScroll = this.catchTimeline - mouseX;
        this.scaleManager.scrollByPixels(moveScroll);
        this.catchTimeline = mouseX;
        return (moveScroll !== 0 );
    }
};
TimelineActions.prototype.timelineDragEnd = function(){
    if(this.catchTimeline) {
        this.scaleManager.releaseWatching();
        this.catchTimeline = false;
    }
};



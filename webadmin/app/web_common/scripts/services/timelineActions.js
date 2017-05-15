'use strict';

function TimelineActions(timelineConfig, positionProvider, scaleManager, animationState, animateScope, scope ) {
    this.animateScope = animateScope;
    this.scope = scope;
    this.positionProvider = positionProvider;
    this.scaleManager = scaleManager;
    this.animationState = animationState;
    this.timelineConfig = timelineConfig;

    this.stopDelay = null;
    this.nextPlayedPosition = 0;
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
    var animation = this.animateScope.animating(self.scope, 'lastPlayedPosition');
    if(animation){
        animation.breakAnimation();
    }
};
TimelineActions.prototype.updatePosition = function(){
    var self = this;
    if(self.positionProvider) {

        if(self.nextPlayedPosition ){
            self.stopAnimatingMove();

            if(self.positionProvider.liveMode || self.nextPlayedPosition == self.positionProvider.playedPosition){
                self.scope.lastPlayedPosition = self.nextPlayedPosition;
                self.nextPlayedPosition = 0;
                self.scaleManager.tryToSetLiveDate(self.scope.lastPlayedPosition, self.positionProvider.liveMode, (new Date()).getTime());
            }
            return; // ignore changes until played position wasn't changed
        }

        var intervalMs = Math.abs(self.scope.lastPlayedPosition - self.positionProvider.playedPosition);
        var duration = Math.min(self.timelineConfig.animationDuration, intervalMs);
        var largeJump = intervalMs > 2 * self.timelineConfig.animationDuration;

        if(intervalMs == 0){
            return;
        }
        if (!largeJump || !self.animateScope.animating(self.scope, 'lastPlayedPosition')) { // Large jump
            self.animateScope.animate(self.scope, 'lastPlayedPosition', self.positionProvider.playedPosition,
                largeJump ? 'smooth' : 'linear', duration).then(
                function () {},
                function () {},
                function (value) {
                    if(self.nextPlayedPosition){
                        console.log("problem with playing position",value,self.nextPlayedPosition);
                        return;
                    }
                    self.scaleManager.tryToSetLiveDate(value, self.positionProvider.liveMode, (new Date()).getTime());
                });
        }
    }
};

TimelineActions.prototype.setAnchorCoordinate = function(mouseX){
    this.delayWatchingPlayingPosition();
    this.stopAnimatingMove();

    this.nextPlayedPosition = this.scaleManager.screenCoordinateToDate(mouseX);
    this.scaleManager.setAnchorCoordinate(mouseX);// Set position to keep

    return this.nextPlayedPosition;
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

//Absolute zoom - to target level from 0 to 1
TimelineActions.prototype.zoomTo = function(zoomTarget, zoomDate, instant, linear){
    var self = this;
    zoomTarget = this.scaleManager.boundZoom(zoomTarget);

    var zoom = self.scaleManager.zoom();
    if(zoom == zoomTarget){
        return;
    }

    function levelsChanged(newLevels,oldLevels){
        if(newLevels && (!oldLevels || !oldLevels.labels)){
            return true;
        }

        if(newLevels.labels.index != oldLevels.labels.index) {
            return true;
        }

        if(newLevels.middle.index != oldLevels.middle.index) {
            return true;
        }

        if(newLevels.small.index != oldLevels.small.index) {
            return true;
        }

        if(newLevels.marks.index != oldLevels.marks.index) {
            return true;
        }

        return false;
    }

    //Find final levels for this zoom and run animation:
    var newTargetLevels = self.scaleManager.targetLevels(zoomTarget);
    if(levelsChanged(newTargetLevels, self.animationState.targetLevels)){
        self.animationState.targetLevels = newTargetLevels;

        if( self.animationState.zooming == 1){ // We need to run animation again
            self.animationState.zooming = 0;
        }

        // This allows us to continue (and slowdown, mb) animation every time
        self.zooming = self.animationState.zooming;
        self.animateScope.animate(self.scope,'zooming',1,'dryResistance').then(function(){
            self.animationState.currentLevels = self.scaleManager.levels;
        },function(){
            // ignore animation re-run
        },function(value){
            self.animationState.zooming = value;
        });
    }


    function setZoom(value){
        if (zoomDate) {
            self.scaleManager.zoomAroundDate(
                value,
                zoomDate
            );
        } else {
            self.scaleManager.zoom(value);
        }
        self.scaleManager.checkZoomAsync().then(function(){
            // TODO: here we need to apply the change to scope - call digest if anything changed
            self.scope.$digest();
        });
        self.delayWatchingPlayingPosition();
    }


    if(!instant) {
        if(!self.zoomTarget) {
            self.zoomTarget = self.scaleManager.zoom();
        }

        self.delayWatchingPlayingPosition();
        self.animateScope.animate(self.scope, 'zoomTarget', zoomTarget, linear?'linear':'dryResistance').then(
            function () {},
            function () {},
            setZoom);
    }else{
        setZoom(zoomTarget);
        self.zoomTarget = self.scaleManager.zoom();
    }
};

//Relative zoom (step)
TimelineActions.prototype.zoom = function(zoomIn,slow,linear, zoomDate){
    var zoomTarget = this.scaleManager.zoom() - (zoomIn ? 1 : -1) * (slow?this.timelineConfig.slowZoomSpeed:this.timelineConfig.zoomSpeed);
    this.zoomTo(zoomTarget, zoomDate, false, linear);
};



TimelineActions.prototype.zoomingRenew = function(){ // renew zooming
    if(this.zoomingNow) {
        this.zoom(this.zoomingIn, true, false);
    }
};

TimelineActions.prototype.zoomingStop = function() {
    if( this.zoomingNow) {
        this.zoomingNow = false;
        this.zoom( this.zoomingIn, true, true);
    }
};

TimelineActions.prototype.zoomingStart = function(zoomIn) {
    if(this.scaleManager.disableZoomOut&&!zoomIn || this.scaleManager.disableZoomIn&&zoomIn){
        return;
    }

    this.zoomingNow = true;
    this.zoomingIn = zoomIn;

    this.zoomingRenew();
};

TimelineActions.prototype.fullZoomOut = function(){
    this.zoomingStop();
    this.zoomTo(1);
};

TimelineActions.prototype.zoomByWheel = function(clicks, mouseOverElements, mouseXOverTimeline){

    var zoom = this.scaleManager.zoom();

    if(window.jscd.touch ) {
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

    var zoomDate = this.scaleManager.screenCoordinateToDate(mouseXOverTimeline);
    if(mouseOverElements.rightBorder && !mouseOverElements.rightButton){
        zoomDate = this.scaleManager.end;
    }
    if(mouseOverElements.leftBorder && !mouseOverElements.leftButton){
        zoomDate = this.scaleManager.start;
    }

    this.zoomTo(this.zoomByWheelTarget, zoomDate, window.jscd.touch);
};

TimelineActions.prototype.updateState = function(){
    this.updatePosition();
    this.zoomingRenew();
    this.scrollingRenew();
};


TimelineActions.prototype.zoomInToPoint = function(mouseX) {
    this.zoom(true, false, false, this.scaleManager.setAnchorCoordinate(mouseX));
};



TimelineActions.prototype.scrollbarSliderDragStart = function(mouseX){
    this.scaleManager.stopWatching();
    this.catchScrollBar = mouseX;
};
TimelineActions.prototype.scrollbarSliderDrag = function(mouseX){
    if(this.catchScrollBar) {
        var moveScroll = mouseX - this.catchScrollBar;
        this.scaleManager.scroll(this.scaleManager.scroll() + moveScroll / this.scaleManager.viewportWidth);
        this.catchScrollBar = mouseX;
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


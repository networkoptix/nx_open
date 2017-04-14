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
}
TimelineActions.prototype.setPositionProvider = function (positionProvider){
    this.positionProvider = positionProvider;
};

TimelineActions.prototype.goToLive = function(force){
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
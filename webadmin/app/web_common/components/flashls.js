function FlashlsAPI (flashObject) {
    function genCallback(flo, p){
        window[p] = function(eventName, args) {
            flo.embedHandler();
            if(flo.flashlsEvents[eventName]) {
                flo.flashlsEvents[eventName].apply(flo, args);
            }
        };
    }

    this.readyToPlay = function(){
        readyToPlay = true;
        if(urlToPlay){
            this.play();
        }
    };

    this.init = function(id, readyHandler,errorHandler, positionHandler) {
        this.id = id;
        this.readyHandler = readyHandler;
        this.errorHandler = errorHandler;
        this.positionHandler = positionHandler;
        genCallback(this, id);
    };

    this.constructor(flashObject);

    var readyToPlay = false;
    var urlToPlay = null;
    this.load = function(url) {
        readyToPlay = false;
        urlToPlay = url;
        if(url.indexOf("http")!=0){
            var server_url = window.location.protocol + "//" + window.location.host + url;
            url = server_url + url;
        }
        this.flashObject.playerLoad(url);
    };

    this.play = function(offset) {
        if(!readyToPlay){
            return;
        }
        this.flashObject.playerPlay(offset || -1);
    };
    
    this.flashlsEvents = {
        ready: function(flashTime) {
            //console.log('ready',flashTime);
        },
        videoSize: function(width, height) {
            //console.log('videoSize',width, height);
        },
        complete: function() {
            this.positionHandler(null);
        },
        error: function(code, url, message) {
            this.errorHandler({message:message,code:code,url:url});
            console.error('flashls error, code:'+ code + ' url:' + url + ' message:' + message);
        },
        manifest: function(duration, levels, loadmetrics) {
            this.readyToPlay();
        },
        audioLevelLoaded: function(loadmetrics) {},
        levelLoaded: function(loadmetrics) {},
        fragmentLoaded: function(loadmetrics) {},
        fragmentPlaying: function(playmetrics) {},
        position: function(timemetrics) {
            this.positionHandler( timemetrics.watched );
        },
        state: function(newState) {},
        seekState: function(newState) {},
        switch: function(newLevel) {},
        audioTracksListChange: function(trackList) {},
        audioTrackChange: function(trackId) {},
        id3Updated: function(ID3Data) {},
        requestPlaylist: function(data) {},
        abortPlaylist: function(data) {},
        requestFragment: function(data) {},
        abortFragment: function(data) {}
    };
}

FlashlsAPI.prototype.kill = function(){
    this.flashObject = null;
};

FlashlsAPI.prototype.ready = function(){
    return !!this.flashObject;
};

FlashlsAPI.prototype.getFlashMovieObject = function (){
    var movieName = 'flashvideoembed_'+this.id;

    if (window.document[movieName])
    {
        return window.document[movieName];
    }
    if (navigator.appName.indexOf('Microsoft Internet')==-1)
    {
        if (document.embeds && document.embeds[movieName])
            return document.embeds[movieName];
    }
    else // if (navigator.appName.indexOf('Microsoft Internet')!=-1)
    {
        return document.getElementById(movieName);
    }
};

FlashlsAPI.prototype.embedHandler = function (e){
    if(!this.flashObject) {
        this.flashObject = this.getFlashMovieObject(); //e.ref is a pointer to the <object>
        this.flashObject.playerSetLogDebug2(true);
        this.readyHandler(this);
    }
};

FlashlsAPI.prototype.constructor = function(flashObject) {
    this.flashObject = flashObject;
};

FlashlsAPI.prototype.pause = function() {
    this.flashObject.playerPause();
};

FlashlsAPI.prototype.resume = function() {
    this.flashObject.playerResume();
};

FlashlsAPI.prototype.seek = function(offset) {
    this.flashObject.playerSeek(offset);
};

FlashlsAPI.prototype.stop = function() {
    this.flashObject.playerStop();
};

FlashlsAPI.prototype.volume = function(volume) {
    this.flashObject.playerVolume(volume);
};

FlashlsAPI.prototype.setCurrentLevel = function(level) {
    this.flashObject.playerSetCurrentLevel(level);
};

FlashlsAPI.prototype.setNextLevel = function(level) {
    this.flashObject.playerSetNextLevel(level);
};

FlashlsAPI.prototype.setLoadLevel = function(level) {
    this.flashObject.playerSetLoadLevel(level);
};

FlashlsAPI.prototype.setMaxBufferLength = function(len) {
    this.flashObject.playerSetmaxBufferLength(len);
};

FlashlsAPI.prototype.getPosition = function() {
    return this.flashObject.getPosition();
};

FlashlsAPI.prototype.getDuration = function() {
    return this.flashObject.getDuration();
};

FlashlsAPI.prototype.getbufferLength = function() {
    return this.flashObject.getbufferLength();
};

FlashlsAPI.prototype.getbackBufferLength = function() {
    return this.flashObject.getbackBufferLength();
};

FlashlsAPI.prototype.getLowBufferLength = function() {
    return this.flashObject.getlowBufferLength();
};

FlashlsAPI.prototype.getMinBufferLength = function() {
    return this.flashObject.getminBufferLength();
};

FlashlsAPI.prototype.getMaxBufferLength = function() {
    return this.flashObject.getmaxBufferLength();
};

FlashlsAPI.prototype.getLevels = function() {
    return this.flashObject.getLevels();
};

FlashlsAPI.prototype.getAutoLevel = function() {
    return this.flashObject.getAutoLevel();
};

FlashlsAPI.prototype.getCurrentLevel = function() {
    return this.flashObject.getCurrentLevel();
};
FlashlsAPI.prototype.getNextLevel = function() {
    return this.flashObject.getNextLevel();
};
FlashlsAPI.prototype.getLoadLevel = function() {
    return this.flashObject.getLoadLevel();
};
FlashlsAPI.prototype.getAudioTrackList = function() {
    return this.flashObject.getAudioTrackList();
};
FlashlsAPI.prototype.getStats = function() {
    return this.flashObject.getStats();
};
FlashlsAPI.prototype.setAudioTrack = function(trackId) {
    this.flashObject.playerSetAudioTrack(trackId);
};
FlashlsAPI.prototype.playerSetLogDebug = function(state) {
    this.flashObject.playerSetLogDebug(state);
};
FlashlsAPI.prototype.getLogDebug = function() {
    return this.flashObject.getLogDebug();
};
FlashlsAPI.prototype.playerSetLogDebug2 = function(state) {
    this.flashObject.playerSetLogDebug2(state);
};
FlashlsAPI.prototype.getLogDebug2 = function() {
    return this.flashObject.getLogDebug2();
};
FlashlsAPI.prototype.playerSetUseHardwareVideoDecoder = function(state) {
    this.flashObject.playerSetUseHardwareVideoDecoder(state);
};
FlashlsAPI.prototype.getUseHardwareVideoDecoder = function() {
    return this.flashObject.getUseHardwareVideoDecoder();
};
FlashlsAPI.prototype.playerSetflushLiveURLCache = function(state) {
    this.flashObject.playerSetflushLiveURLCache(state);
};
FlashlsAPI.prototype.getflushLiveURLCache = function() {
    return this.flashObject.getflushLiveURLCache();
};

FlashlsAPI.prototype.playerSetJSURLStream = function(state) {
    this.flashObject.playerSetJSURLStream(state);
};

FlashlsAPI.prototype.getJSURLStream = function() {
    return this.flashObject.getJSURLStream();
};

FlashlsAPI.prototype.playerCapLeveltoStage = function(state) {
    this.flashObject.playerCapLeveltoStage(state);
};

FlashlsAPI.prototype.getCapLeveltoStage = function() {
    return this.flashObject.getCapLeveltoStage();
};

FlashlsAPI.prototype.playerSetAutoLevelCapping = function(level) {
    this.flashObject.playerSetAutoLevelCapping(level);
};

FlashlsAPI.prototype.getAutoLevelCapping = function() {
    return this.flashObject.getAutoLevelCapping();
};

var flashlsAPI = new (function(flashObject) {

    this.kill = function(){
        this.flashObject = null;
    };
    this.ready = function(){
        return !!this.flashObject;
    };

    this.getFlashMovieObject = function (){
        var movieName = 'flashvideoembed';

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
    this.embedHandler = function (e){
        if(!this.flashObject) {
            this.flashObject = this.getFlashMovieObject(); //e.ref is a pointer to the <object>
            this.flashObject.playerSetLogDebug2(true);
            this.readyHandler(this);
        }
    };

    this.readyToPlay = function(){
        readyToPlay = true;
        if(urlToPlay){
            this.play();
        }
    };

    this.init = function(element,readyHandler,errorHandler, positionHandler) {
        this.element = element;
        this.readyHandler = readyHandler;
        this.errorHandler = errorHandler;
        this.positionHandler = positionHandler;
    };

    this.constructor = function(flashObject) {
        this.flashObject = flashObject;
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

    this.pause = function() {
        this.flashObject.playerPause();
    };

    this.resume = function() {
        this.flashObject.playerResume();
    };

    this.seek = function(offset) {
        this.flashObject.playerSeek(offset);
    };

    this.stop = function() {
        this.flashObject.playerStop();
    };

    this.volume = function(volume) {
        this.flashObject.playerVolume(volume);
    };

    this.setCurrentLevel = function(level) {
        this.flashObject.playerSetCurrentLevel(level);
    };

    this.setNextLevel = function(level) {
        this.flashObject.playerSetNextLevel(level);
    };

    this.setLoadLevel = function(level) {
        this.flashObject.playerSetLoadLevel(level);
    };

    this.setMaxBufferLength = function(len) {
        this.flashObject.playerSetmaxBufferLength(len);
    };

    this.getPosition = function() {
        return this.flashObject.getPosition();
    };

    this.getDuration = function() {
        return this.flashObject.getDuration();
    };

    this.getbufferLength = function() {
        return this.flashObject.getbufferLength();
    };

    this.getbackBufferLength = function() {
        return this.flashObject.getbackBufferLength();
    };

    this.getLowBufferLength = function() {
        return this.flashObject.getlowBufferLength();
    };

    this.getMinBufferLength = function() {
        return this.flashObject.getminBufferLength();
    };

    this.getMaxBufferLength = function() {
        return this.flashObject.getmaxBufferLength();
    };

    this.getLevels = function() {
        return this.flashObject.getLevels();
    };

    this.getAutoLevel = function() {
        return this.flashObject.getAutoLevel();
    };

    this.getCurrentLevel = function() {
        return this.flashObject.getCurrentLevel();
    };

    this.getNextLevel = function() {
        return this.flashObject.getNextLevel();
    };

    this.getLoadLevel = function() {
        return this.flashObject.getLoadLevel();
    };

    this.getAudioTrackList = function() {
        return this.flashObject.getAudioTrackList();
    };

    this.getStats = function() {
        return this.flashObject.getStats();
    };

    this.setAudioTrack = function(trackId) {
        this.flashObject.playerSetAudioTrack(trackId);
    };

    this.playerSetLogDebug = function(state) {
        this.flashObject.playerSetLogDebug(state);
    };

    this.getLogDebug = function() {
        return this.flashObject.getLogDebug();
    };

    this.playerSetLogDebug2 = function(state) {
        this.flashObject.playerSetLogDebug2(state);
    };

    this.getLogDebug2 = function() {
        return this.flashObject.getLogDebug2();
    };

    this.playerSetUseHardwareVideoDecoder = function(state) {
        this.flashObject.playerSetUseHardwareVideoDecoder(state);
    };

    this.getUseHardwareVideoDecoder = function() {
        return this.flashObject.getUseHardwareVideoDecoder();
    };

    this.playerSetflushLiveURLCache = function(state) {
        this.flashObject.playerSetflushLiveURLCache(state);
    };

    this.getflushLiveURLCache = function() {
        return this.flashObject.getflushLiveURLCache();
    };

    this.playerSetJSURLStream = function(state) {
        this.flashObject.playerSetJSURLStream(state);
    };

    this.getJSURLStream = function() {
        return this.flashObject.getJSURLStream();
    };

    this.playerCapLeveltoStage = function(state) {
        this.flashObject.playerCapLeveltoStage(state);
    };

    this.getCapLeveltoStage = function() {
        return this.flashObject.getCapLeveltoStage();
    };

    this.playerSetAutoLevelCapping = function(level) {
        this.flashObject.playerSetAutoLevelCapping(level);
    };

    this.getAutoLevelCapping = function() {
        return this.flashObject.getAutoLevelCapping();
    };
})();

flashlsAPI.flashlsEvents = {
    ready: function(flashTime) {
        //console.log('ready',flashTime);
    },
    videoSize: function(width, height) {
        //console.log('videoSize',width, height);
    },
    complete: function() {
        flashlsAPI.positionHandler(null);
    },
    error: function(code, url, message) {
        flashlsAPI.errorHandler({message:message,code:code,url:url});
        console.error('flashls error, code:'+ code + ' url:' + url + ' message:' + message);
    },
    manifest: function(duration, levels, loadmetrics) {
        flashlsAPI.readyToPlay();
    },
    audioLevelLoaded: function(loadmetrics) {},
    levelLoaded: function(loadmetrics) {},
    fragmentLoaded: function(loadmetrics) {},
    fragmentPlaying: function(playmetrics) {},
    position: function(timemetrics) {
        flashlsAPI.positionHandler( timemetrics.watched );
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


window.flashlsCallback = function(eventName, args) {
    flashlsAPI.embedHandler();
    if(flashlsAPI.flashlsEvents[eventName]) {
        flashlsAPI.flashlsEvents[eventName].apply(null, args);
    }
};
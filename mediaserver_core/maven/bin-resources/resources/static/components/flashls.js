'use strict';
/*var JSLoaderFragment = {

    requestFragment : function(instanceId,url, resourceLoadedFlashCallback, resourceFailureFlashCallback) {
        if(!this.flashObject) {
            this.flashObject = getFlashMovieObject(instanceId);
        }
        this.xhrGET(url,this.xhrReadBytes, this.xhrTransferFailed, resourceLoadedFlashCallback, resourceFailureFlashCallback, "arraybuffer");
    },
    abortFragment : function(instanceId) {
        if(this.xhr &&this.xhr.readyState !== 4) {
            this.xhr.abort();
        }
    },
    xhrGET : function (url,loadcallback, errorcallback,resourceLoadedFlashCallback, resourceFailureFlashCallback, responseType) {
        var xhr = new XMLHttpRequest();
        this.xhr = xhr;
        xhr.binding = this;
        xhr.resourceLoadedFlashCallback = resourceLoadedFlashCallback;
        xhr.resourceFailureFlashCallback = resourceFailureFlashCallback;
        xhr.open("GET", url, loadcallback? true: false);
        if (responseType) {
            xhr.responseType = responseType;
        }
        if (loadcallback) {
            xhr.onload = loadcallback;
            xhr.onerror= errorcallback;
            xhr.send();
        } else {
            xhr.send();
            return xhr.status == 200? xhr.response: "";
        }
    },
    xhrReadBytes : function(event) {
        var len = event.currentTarget.response.byteLength;
        var t0 = new Date();
        var res = base64ArrayBuffer(event.currentTarget.response);
        var t1 = new Date();
        this.binding.flashObject[this.resourceLoadedFlashCallback](res,len);
        var t2 = new Date();
        ////console.log('encoding/toFlash:' + (t1-t0) + '/' + (t2-t1));
        ////console.log('encoding speed/toFlash:' + Math.round(len/(t1-t0)) + 'kB/s/' + Math.round(res.length/(t2-t1)) + 'kB/s');
    },
    xhrTransferFailed : function(oEvent) {
        ////console.log("An error occurred while transferring the file :" + oEvent.target.status);
        this.binding.flashObject[this.resourceFailureFlashCallback](res);
    }
};

var JSLoaderPlaylist = {

    requestPlaylist : function(instanceId,url, resourceLoadedFlashCallback, resourceFailureFlashCallback) {
        ////console.log("JSURLLoader.onRequestResource");
        if(!this.flashObject) {
            this.flashObject = getFlashMovieObject(instanceId);
        }
        this.xhrGET(url,this.xhrReadBytes, this.xhrTransferFailed, resourceLoadedFlashCallback, resourceFailureFlashCallback);
    },
    abortPlaylist : function(instanceId) {
        if(this.xhr &&this.xhr.readyState !== 4) {
            this.xhr.abort();
        }
    },
    xhrGET : function (url,loadcallback, errorcallback,resourceLoadedFlashCallback, resourceFailureFlashCallback) {
        var xhr = new XMLHttpRequest();
        xhr.binding = this;
        this.xhr = xhr;
        xhr.resourceLoadedFlashCallback = resourceLoadedFlashCallback;
        xhr.resourceFailureFlashCallback = resourceFailureFlashCallback;
        xhr.open("GET", url, loadcallback? true: false);
        if (loadcallback) {
            xhr.onload = loadcallback;
            xhr.onerror= errorcallback;
            xhr.send();
        } else {
            xhr.send();
            return xhr.status == 200? xhr.response: "";
        }
    },
    xhrReadBytes : function(event) {
        ////console.log("playlist loaded");
        this.binding.flashObject[this.resourceLoadedFlashCallback](event.currentTarget.responseText);
    },
    xhrTransferFailed : function(oEvent) {
        ////console.log("An error occurred while transferring the file :" + oEvent.target.status);
        this.binding.flashObject[this.resourceFailureFlashCallback](res);
    }
};*/



var flashlsAPI = new (function(){

    //We use swfobject to embed player. see https://code.google.com/p/swfobject/wiki/documentation
    var flashParameters = {
        player:"components/flashlsChromeless.swf",
        id:"flashvideo",
        name:"flashvideoembed",
        width:"100%",
        height:"100%",
        version: "9.0.0",
        installer:"components/expressInstall.swf",
        flashvars:{

        },
        params:{
            //movie:"components/flashlsChromeless.swf",
            quality:"high",
            swliveconnect:true,
            allowScriptAccess:"always",
            bgcolor:"#1C2327",
            allowFullScreen:false,
            wmode:"window",
            FlashVars:"callback=flashlsCallback"
        },
        attributes:{ // https://code.google.com/p/swfobject/wiki/documentation - see section "How can you configure your Flash content?
            id:"flashvideowindow",
            name: "flashvideoembed"
            /*,
            align:"",
            name:"",
            styleclass:"",
            class:""*/
        }
    };
    this.kill = function(){
        this.flashObject = null;
    };
    this.ready = function(){
        return !!this.flashObject;
    };

    this.flashParams = function(){
        return flashParameters.params;
    };

    this.getFlashMovieObject = function (){
        var movieName = flashParameters.name || this.element;
        if (window.document[movieName])
        {
            return window.document[movieName];
        }
        if (navigator.appName.indexOf("Microsoft Internet")==-1)
        {
            if (document.embeds && document.embeds[movieName])
                return document.embeds[movieName];
        }
        else // if (navigator.appName.indexOf("Microsoft Internet")!=-1)
        {
            return document.getElementById(movieName);
        }
    };
    this.embedHandler = function (e){
        if(!this.flashObject) {
            this.flashObject = this.getFlashMovieObject(); //e.ref is a pointer to the <object>

            //console.log("embed handler");
            //console.log(this.flashObject);

            if(window.jscd.browser == 'Microsoft Internet Explorer' && window.jscd.browserMajorVersion<=10) { // Force disabling hardware acceleration for IE9, IE10
                this.playerSetUseHardwareVideoDecoder(false);
            }
            this.readyHandler(this);
        }
    };

    this.init = function(element,readyHandler,errorHandler, positionHandler) {
        this.element = element;
        var self = this;

        this.readyHandler = readyHandler;
        this.errorHandler = errorHandler;
        this.positionHandler = positionHandler;

        /*swfobject.embedSWF(
            flashParameters.player,
            element,
            flashParameters.width,
            flashParameters.height,
            flashParameters.version,
            flashParameters.installer,
            flashParameters.flashvars,
            flashParameters.params,
            flashParameters.attributes,
            function(){
                //console.log("swf ready");
            });*/

    };

    
    this.load = function(url) {
        if(!url){
            return;
        }
        //console.log("load video",url);
        try {
            this.flashObject.playerLoad(url);
        }catch(a){
            this.flashObject = null;
            this.errorHandler(a);
        }
    };

    this.play = function(offset) {
        if(!this.flashObject){
            return;
        }
        try {
            this.flashObject.playerPlay(offset);
        }catch(a){
            this.flashObject = null;
            this.errorHandler(a);
        }
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
var timer = 0;
flashlsAPI.flashlsEvents = {
    ready: function(flashTime) {
        //console.log("ready",flashTime);
    },
    videoSize: function(width, height) {
        //console.log("videoSize",width, height);
    },
    complete: function() {
        //console.log("onComplete(), playback completed");
    },
    error: function(code, url, message) {
        //console.log("onError():error code:"+ code + " url:" + url + " message:" + message);
    },
    manifest: function(duration, levels, loadmetrics) {
        //console.log("manifest loaded, playlist duration:"+ duration.toFixed(2));
    },
    audioLevelLoaded: function(loadmetrics) {
        var event = {
            type : "level audio",
            id : loadmetrics.level,
            start : loadmetrics.id,
            end : loadmetrics.id2,
            latency : loadmetrics.loading_begin_time - loadmetrics.loading_request_time,
            load : loadmetrics.loading_end_time - loadmetrics.loading_begin_time,
            duration : loadmetrics.loading_end_time - loadmetrics.loading_begin_time,
            bw : Math.round(loadmetrics.bandwidth/1000)
        };
        event.time = loadmetrics.loading_request_time - flashLoadDate;
        //console.log("audioLevelLoaded",event.time);
    },
    levelLoaded: function(loadmetrics) {
        var event = {
            type : "level",
            id : loadmetrics.level,
            start : loadmetrics.id,
            end : loadmetrics.id2,
            latency : loadmetrics.loading_begin_time - loadmetrics.loading_request_time,
            load : loadmetrics.loading_end_time - loadmetrics.loading_begin_time,
            duration : loadmetrics.loading_end_time - loadmetrics.loading_begin_time,
            bw : Math.round(loadmetrics.bandwidth/1000)
        };
        event.time = loadmetrics.loading_request_time - flashLoadDate;

        //console.log("levelLoaded",event);
    },
    fragmentLoaded: function(loadmetrics) {
        var event = {
            id : loadmetrics.level,
            id2 : loadmetrics.id,
            latency : loadmetrics.loading_begin_time - loadmetrics.loading_request_time,
            load : loadmetrics.loading_end_time - loadmetrics.loading_begin_time,
            demux : loadmetrics.parsing_end_time - loadmetrics.loading_end_time,
            duration : loadmetrics.parsing_end_time - loadmetrics.loading_begin_time,
            bw : Math.round(loadmetrics.bandwidth/1000)
        };
        event.time = loadmetrics.loading_request_time - flashLoadDate;
        if(loadmetrics.type == 4) {
            event.type =  'fragment audio'
        } else {
            event.type =  'fragment main'
        }
        //console.log("fragmentLoaded",event);
    },
    fragmentPlaying: function(playmetrics) {
        var event = {time : new Date() - jsLoadDate, type : "playing frag", name : playmetrics.seqnum + '@' + playmetrics.level, duration : playmetrics.duration*1000};

        //console.log("fragmentPlaying",event);
    },
    position: function(timemetrics) {


        var backbuffer = timemetrics.backbuffer;
        // TODO: what if video stopped? (buffer goes 0);
        flashlsAPI.positionHandler(Math.round(backbuffer * 1000))

        if(!timer){
            timer = (new Date()).getTime();
        }
        /*var position = timemetrics.position;
        var duration = timemetrics.duration;
        var sliding = timemetrics.live_sliding_main;
        var buffer = timemetrics.buffer;

        //console.log("position changed",
            (((new Date()).getTime() - timer) / 1000 ).toFixed(2),
            backbuffer.toFixed(2),
            position.toFixed(2),
            "bb:",backbuffer.toFixed(2),
            (backbuffer - position).toFixed(2),
            (backbuffer + position).toFixed(2),
            (buffer + backbuffer).toFixed(2)
        );*/
    },
    state: function(newState) {
        var event = {time : new Date() - jsLoadDate, type : newState.toLowerCase(), name : ''};

        //console.log("state",event);
    },
    seekState: function(newState) {
        var event = {time : new Date() - jsLoadDate, type : newState.toLowerCase(), name : ''};

        //console.log("seekState",event);
    },
    switch: function(newLevel) {
        var event = {time : new Date() - jsLoadDate, type : "levelSwitch", name : newLevel.toFixed()};

        //console.log("switch",event);
    },
    audioTracksListChange: function(trackList) {

        //console.log("audioTracksListChange",trackList);
    },
    audioTrackChange: function(trackId) {
        var event = {time : new Date() - jsLoadDate, type : "audioTrackChange", name : trackId.toFixed()};

        //console.log("audioTrackChange",event);
    },
    id3Updated: function(ID3Data) {
        var event = {time : new Date() - jsLoadDate, type : "ID3Data", id3data: atob(ID3Data)};

        //console.log("id3Updated",event);
    }
    /*,
    requestPlaylist: JSLoaderPlaylist.requestPlaylist.bind(JSLoaderPlaylist),
    abortPlaylist: JSLoaderPlaylist.abortPlaylist.bind(JSLoaderPlaylist),
    requestFragment: JSLoaderFragment.requestFragment.bind(JSLoaderFragment),
    abortFragment: JSLoaderFragment.abortFragment.bind(JSLoaderFragment)
    */
};


window.flashlsCallback = function(eventName, args) {
    if(!flashlsAPI.flashObject) {
        //console.log("flashlsCallback ", eventName, args);
    }
    flashlsAPI.embedHandler();
    if(flashlsAPI.flashlsEvents[eventName]) {
        flashlsAPI.flashlsEvents[eventName].apply(null, args);
    }
};
'use strict';
var JSLoaderFragment = {

    requestFragment : function(instanceId,url, resourceLoadedFlashCallback, resourceFailureFlashCallback) {
        //console.log("JSURLStream.onRequestResource");
        if(!this.flashObject) {
            this.flashObject = getFlashMovieObject(instanceId);
        }
        this.xhrGET(url,this.xhrReadBytes, this.xhrTransferFailed, resourceLoadedFlashCallback, resourceFailureFlashCallback, "arraybuffer");
    },
    abortFragment : function(instanceId) {
        if(this.xhr &&this.xhr.readyState !== 4) {
            console.log("JSLoaderFragment:abort XHR");
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
        //console.log("fragment loaded");
        var len = event.currentTarget.response.byteLength;
        var t0 = new Date();
        var res = base64ArrayBuffer(event.currentTarget.response);
        var t1 = new Date();
        this.binding.flashObject[this.resourceLoadedFlashCallback](res,len);
        var t2 = new Date();
        console.log('encoding/toFlash:' + (t1-t0) + '/' + (t2-t1));
        console.log('encoding speed/toFlash:' + Math.round(len/(t1-t0)) + 'kB/s/' + Math.round(res.length/(t2-t1)) + 'kB/s');
    },
    xhrTransferFailed : function(oEvent) {
        console.log("An error occurred while transferring the file :" + oEvent.target.status);
        this.binding.flashObject[this.resourceFailureFlashCallback](res);
    }
};

var JSLoaderPlaylist = {

    requestPlaylist : function(instanceId,url, resourceLoadedFlashCallback, resourceFailureFlashCallback) {
        //console.log("JSURLLoader.onRequestResource");
        if(!this.flashObject) {
            this.flashObject = getFlashMovieObject(instanceId);
        }
        this.xhrGET(url,this.xhrReadBytes, this.xhrTransferFailed, resourceLoadedFlashCallback, resourceFailureFlashCallback);
    },
    abortPlaylist : function(instanceId) {
        if(this.xhr &&this.xhr.readyState !== 4) {
            console.log("JSLoaderPlaylist:abort XHR");
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
        //console.log("playlist loaded");
        this.binding.flashObject[this.resourceLoadedFlashCallback](event.currentTarget.responseText);
    },
    xhrTransferFailed : function(oEvent) {
        console.log("An error occurred while transferring the file :" + oEvent.target.status);
        this.binding.flashObject[this.resourceFailureFlashCallback](res);
    }
};



var flashlsAPI = new (function(){

    //We use swfobject to embed player. see https://code.google.com/p/swfobject/wiki/documentation
    var flashParameters = {
        player:"components/flashlsChromeless.swf",
        id:"flashlsPlayer",
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
            allowScriptAccess:"sameDomain",
            bgcolor:"#0",
            allowFullScreen:true,
            wmode:"window"
            //FlashVars:"callback=flashlsCallback"
        },
        attributes:{ // https://code.google.com/p/swfobject/wiki/documentation - see section "How can you configure your Flash content?
            id:"flashvideowindow"/*,
            align:"",
            name:"",
            styleclass:"",
            class:""*/
        }
    };

    this.getFlashMovieObject = function (){
        var movieName = flashParameters.attributes.id || this.element;
        if (window.document[movieName])
        {
            return window.document[movieName];
        }
        if (navigator.appName.indexOf("Microsoft Internet")==-1)
        {
            console.log(document.embeds);
            if (document.embeds && document.embeds[movieName])
                return document.embeds[movieName];
        }
        else // if (navigator.appName.indexOf("Microsoft Internet")!=-1)
        {
            return document.getElementById(movieName);
        }
    };

    this.init = function(element,readyHandler,errorHandler) {
        this.element = element;
        var self = this;
        var embedHandler = function (e){
            if(e.success) {
                self.flashObject = self.getFlashMovieObject(); //e.ref is a pointer to the <object>

                console.log("embedHandler", self.flashObject);
                //do something with mySWF
                readyHandler(self);
            }else{
                if(errorHandler) {
                    errorHandler(self);
                }
            }
        };
        this.readyHandler = readyHandler;
        this.errorHandler = errorHandler;
        flashParameters.flashvars.callback = "flashlsCallback";

        swfobject.embedSWF(
            flashParameters.player,
            element,
            flashParameters.width,
            flashParameters.height,
            flashParameters.version,
            flashParameters.installer,
            flashParameters.flashvars,
            flashParameters.params,
            flashParameters.attributes,
            embedHandler);
    };

    
    this.load = function(url) {
        console.log("load", url);
        this.flashObject.playerLoad(url);
        this.play();
    };

    this.play = function(offset) {
        this.flashObject.playerPlay(offset);
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
        //flashPingDate = flashTime;
        //jsPingDate = new Date();
        document.getElementById('mediaInfo').rows[Y_LOW_BUFFER].cells[X_LOW_BUFFER].innerHTML = api.getLowBufferLength().toFixed(2);
        document.getElementById('mediaInfo').rows[Y_MIN_BUFFER].cells[X_MIN_BUFFER].innerHTML = api.getMinBufferLength().toFixed(2);
        document.getElementById('mediaInfo').rows[Y_MAX_BUFFER].cells[X_MAX_BUFFER].innerHTML = api.getMaxBufferLength().toFixed(2);

        //this.flashObject = this.getFlashMovieObject();
        //this.readyHandler(this);

        console.log("ready",this);
        //api = new flashlsAPI(getFlashMovieObject());
    },
    videoSize: function(width, height) {
        var event = {time : new Date() - jsLoadDate, type : "resize", name : width + 'x' + height};
        events.video.push(event);
        var state = api.getCapLeveltoStage();
        if(!state) {
            var ratio = width / height;
            if (height > window.innerHeight-30) {
                height = window.innerHeight-30;
                width = Math.round(height * ratio);
            }
            api.flashObject.width = width;
            api.flashObject.height = height;
            var canvas = document.getElementById('buffered_c');
            canvas.width = width;
        }
    },
    complete: function() {
        appendLog("onComplete(), playback completed");
    },
    error: function(code, url, message) {
        appendLog("onError():error code:"+ code + " url:" + url + " message:" + message);
    },
    manifest: function(duration, levels, loadmetrics) {
        appendLog("manifest loaded, playlist duration:"+ duration.toFixed(2));
        document.getElementById('mediaInfo').rows[Y_DURATION].cells[X_DURATION].innerHTML =  duration.toFixed(2);
        showCanvas();
        updateLevelInfo();
        api.play(-1);
        api.volume(10);
        var event = {
            type : "manifest",
            name : "",
            latency : loadmetrics.loading_begin_time - loadmetrics.loading_request_time,
            load : loadmetrics.loading_end_time - loadmetrics.loading_begin_time,
            duration : loadmetrics.loading_end_time - loadmetrics.loading_begin_time,
            bw : Math.round(loadmetrics.bandwidth/1000)
        };
        event.time = loadmetrics.loading_request_time - flashLoadDate;
        events.load.push(event);
        refreshCanvas();
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
        events.load.push(event);
        refreshCanvas();
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
        events.load.push(event);
        refreshCanvas();
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
        events.load.push(event);
        document.getElementById("HlsStats").innerHTML = JSON.stringify(api.getStats(),null,"\t");
        refreshCanvas();
    },
    fragmentPlaying: function(playmetrics) {
        var event = {time : new Date() - jsLoadDate, type : "playing frag", name : playmetrics.seqnum + '@' + playmetrics.level, duration : playmetrics.duration*1000};
        events.video.push(event);
        updateLevelInfo();
    },
    position: function(timemetrics) {
        document.getElementById('mediaInfo').rows[Y_POSITION].cells[X_POSITION].innerHTML =  timemetrics.position.toFixed(2);
        document.getElementById('mediaInfo').rows[Y_DURATION].cells[X_DURATION].innerHTML =  timemetrics.duration.toFixed(2);
        document.getElementById('mediaInfo').rows[Y_BUFFER].cells[X_BUFFER].innerHTML =  timemetrics.buffer.toFixed(2);
        document.getElementById('mediaInfo').rows[Y_BACK_BUFFER].cells[X_BACK_BUFFER].innerHTML =  timemetrics.backbuffer.toFixed(2);
        document.getElementById('mediaInfo').rows[Y_PLAYLIST_SLIDING].cells[X_PLAYLIST_SLIDING].innerHTML =  timemetrics.live_sliding_main.toFixed(2);

        var event = { time : new Date() - jsLoadDate, buffer : Math.round(1000*timemetrics.buffer), pos: Math.round(1000*timemetrics.position)};
        var bufEvents = events.buffer, bufEventLen = bufEvents.length;
        if(bufEventLen > 1) {
            var event0 = bufEvents[bufEventLen-2],event1 = bufEvents[bufEventLen-1];
            var slopeBuf0 = (event0.buffer - event1.buffer)/(event0.time-event1.time);
            var slopeBuf1 = (event1.buffer - event.buffer)/(event1.time-event.time);

            var slopePos0 = (event0.pos - event1.pos)/(event0.time-event1.time);
            var slopePos1 = (event1.pos - event.pos)/(event1.time-event.time);
            // compute slopes. if less than 30% difference, remove event1
            if((slopeBuf0 === slopeBuf1 || Math.abs(slopeBuf0/slopeBuf1 -1) <= 0.3) &&
                (slopePos0 === slopePos1 || Math.abs(slopePos0/slopePos1 -1) <= 0.3))
            {
                bufEvents.pop();
            }
        }
        events.buffer.push(event);
        updateBufferCanvas(timemetrics.position,timemetrics.duration,timemetrics.buffer, timemetrics.backbuffer);
        refreshCanvas();
    },
    state: function(newState) {
        var event = {time : new Date() - jsLoadDate, type : newState.toLowerCase(), name : ''};
        events.video.push(event);
        document.getElementById('mediaInfo').rows[Y_STATE].cells[X_STATE].innerHTML =  newState;
    },
    seekState: function(newState) {
        var event = {time : new Date() - jsLoadDate, type : newState.toLowerCase(), name : ''};
        events.video.push(event);

        if(event.type === 'seeking') {
            //lastSeekingIdx = events.video.length-1;
        }
        if(event.type === 'seeked') {
            events.video[lastSeekingIdx].duration = event.time - events.video[lastSeekingIdx].time;
        }
    },
    switch: function(newLevel) {
        var event = {time : new Date() - jsLoadDate, type : "levelSwitch", name : newLevel.toFixed()};
        events.video.push(event);
        document.getElementById('mediaInfo').rows[Y_LOAD_LEVEL].cells[X_LOAD_LEVEL].innerHTML =  newLevel;
    },
    audioTracksListChange: function(trackList) {
        var d = document.getElementById('audioControl');
        var html = '';
        appendLog("new track list");
        for (var t in trackList) {
            appendLog("    " + trackList[t].title + " [" + trackList[t].id + "]");
            html += '<button onclick="api.setAudioTrack(' +t+ ')">' + trackList[t].title + '</button>';
        }
        d.innerHTML = html;
    },
    audioTrackChange: function(trackId) {
        var event = {time : new Date() - jsLoadDate, type : "audioTrackChange", name : trackId.toFixed()};
        events.video.push(event);
        document.getElementById('mediaInfo').rows[Y_AUDIOTRACKID].cells[X_AUDIOTRACKID].innerHTML = trackId;
    },
    id3Updated: function(ID3Data) {
        var event = {time : new Date() - jsLoadDate, type : "ID3Data", id3data: atob(ID3Data)};
        events.video.push(event);
    },
    requestPlaylist: JSLoaderPlaylist.requestPlaylist.bind(JSLoaderPlaylist),
    abortPlaylist: JSLoaderPlaylist.abortPlaylist.bind(JSLoaderPlaylist),
    requestFragment: JSLoaderFragment.requestFragment.bind(JSLoaderFragment),
    abortFragment: JSLoaderFragment.abortFragment.bind(JSLoaderFragment)
};


window.flashlsCallback = function(eventName, args) {
    flashlsAPI.flashlsEvents[eventName].apply(null, args);
};
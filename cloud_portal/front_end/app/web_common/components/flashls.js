(function () {
    
    'use strict';
    
    window.FlashlsAPI = function (flashObject) {
        function genCallback(flo, p) {
            window[p] = function (eventName, args) {
                flo.embedHandler();
                if (flo.flashlsEvents[eventName]) {
                    flo.flashlsEvents[eventName].apply(flo, args);
                }
            };
        }
        
        var readyToPlay = false;
        var urlToPlay = null;
        
        this.readyToPlay = function () {
            readyToPlay = true;
            if (urlToPlay) {
                this.play();
            }
        };
        
        this.init = function (id, readyHandler, errorHandler, positionHandler) {
            this.id = id;
            this.readyHandler = readyHandler;
            this.errorHandler = errorHandler;
            this.positionHandler = positionHandler;
            genCallback(this, id);
        };
        
        this.constructor(flashObject);
        
        this.load = function (url) {
            readyToPlay = false;
            urlToPlay = url;
            if (url.indexOf('http') !== 0) {
                var serverUrl = window.location.protocol + '//' + window.location.host + url;
                url = serverUrl + url;
            }
            this.flashObject.playerLoad(url);
        };
        
        this.play = function (offset) {
            if (!readyToPlay) {
                return;
            }
            this.flashObject.playerPlay(offset || -1);
        };
        
        this.flashlsEvents = {
            ready: function (flashTime) {
                //console.log('ready',flashTime);
            },
            videoSize: function (width, height) {
                //console.log('videoSize',width, height);
            },
            complete: function () {
                this.positionHandler(null, null);
            },
            error: function (code, url, message) {
                this.errorHandler({message: message, code: code, url: url});
                console.error('flashls error, code:' + code + ' url:' + url + ' message:' + message);
            },
            manifest: function (duration, levels, loadmetrics) {
                this.readyToPlay();
            },
            audioLevelLoaded: function (loadmetrics) {
            },
            levelLoaded: function (loadmetrics) {
            },
            fragmentLoaded: function (loadmetrics) {
            },
            fragmentPlaying: function (playmetrics) {
            },
            position: function (timemetrics) {
                this.positionHandler(timemetrics.watched);
            },
            state: function (newState) {
            },
            seekState: function (newState) {
            },
            switch: function (newLevel) {
            },
            audioTracksListChange: function (trackList) {
            },
            audioTrackChange: function (trackId) {
            },
            id3Updated: function (ID3Data) {
            },
            requestPlaylist: function (data) {
            },
            abortPlaylist: function (data) {
            },
            requestFragment: function (data) {
            },
            abortFragment: function (data) {
            }
        };
    };
    
    window.FlashlsAPI.prototype.kill = function () {
        delete window[this.id];
        this.flashObject = null;
    };
    
    window.FlashlsAPI.prototype.ready = function () {
        return !!this.flashObject;
    };
    
    window.FlashlsAPI.prototype.getFlashMovieObject = function () {
        var movieName = 'flashvideoembed_' + this.id;
        
        if (window.document[movieName]) {
            return window.document[movieName];
        }
        if (navigator.appName.indexOf('Microsoft Internet') === -1) {
            if (document.embeds && document.embeds[movieName]) {
                return document.embeds[movieName];
            }
        } else { // if (navigator.appName.indexOf('Microsoft Internet')!=-1)
            return document.getElementById(movieName);
        }
    };
    
    window.FlashlsAPI.prototype.embedHandler = function (e) {
        if (!this.flashObject) {
            this.flashObject = this.getFlashMovieObject(); //e.ref is a pointer to the <object>
            this.flashObject.playerSetLogDebug2(true);
            this.readyHandler(this);
        }
    };
    
    window.FlashlsAPI.prototype.constructor = function (flashObject) {
        this.flashObject = flashObject;
    };
    
    window.FlashlsAPI.prototype.pause = function () {
        this.flashObject.playerPause();
    };
    
    window.FlashlsAPI.prototype.resume = function () {
        this.flashObject.playerResume();
    };
    
    window.FlashlsAPI.prototype.seek = function (offset) {
        this.flashObject.playerSeek(offset);
    };
    
    window.FlashlsAPI.prototype.stop = function () {
        this.flashObject.playerStop();
    };
    
    window.FlashlsAPI.prototype.volume = function (volume) {
        this.flashObject.playerVolume(volume);
    };
    
    window.FlashlsAPI.prototype.setCurrentLevel = function (level) {
        this.flashObject.playerSetCurrentLevel(level);
    };
    
    window.FlashlsAPI.prototype.setNextLevel = function (level) {
        this.flashObject.playerSetNextLevel(level);
    };
    
    window.FlashlsAPI.prototype.setLoadLevel = function (level) {
        this.flashObject.playerSetLoadLevel(level);
    };
    
    window.FlashlsAPI.prototype.setMaxBufferLength = function (len) {
        this.flashObject.playerSetmaxBufferLength(len);
    };
    
    window.FlashlsAPI.prototype.getPosition = function () {
        return this.flashObject.getPosition();
    };
    
    window.FlashlsAPI.prototype.getDuration = function () {
        return this.flashObject.getDuration();
    };
    
    window.FlashlsAPI.prototype.getbufferLength = function () {
        return this.flashObject.getbufferLength();
    };
    
    window.FlashlsAPI.prototype.getbackBufferLength = function () {
        return this.flashObject.getbackBufferLength();
    };
    
    window.FlashlsAPI.prototype.getLowBufferLength = function () {
        return this.flashObject.getlowBufferLength();
    };
    
    window.FlashlsAPI.prototype.getMinBufferLength = function () {
        return this.flashObject.getminBufferLength();
    };
    
    window.FlashlsAPI.prototype.getMaxBufferLength = function () {
        return this.flashObject.getmaxBufferLength();
    };
    
    window.FlashlsAPI.prototype.getLevels = function () {
        return this.flashObject.getLevels();
    };
    
    window.FlashlsAPI.prototype.getAutoLevel = function () {
        return this.flashObject.getAutoLevel();
    };
    
    window.FlashlsAPI.prototype.getCurrentLevel = function () {
        return this.flashObject.getCurrentLevel();
    };
    window.FlashlsAPI.prototype.getNextLevel = function () {
        return this.flashObject.getNextLevel();
    };
    window.FlashlsAPI.prototype.getLoadLevel = function () {
        return this.flashObject.getLoadLevel();
    };
    window.FlashlsAPI.prototype.getAudioTrackList = function () {
        return this.flashObject.getAudioTrackList();
    };
    window.FlashlsAPI.prototype.getStats = function () {
        return this.flashObject.getStats();
    };
    window.FlashlsAPI.prototype.setAudioTrack = function (trackId) {
        this.flashObject.playerSetAudioTrack(trackId);
    };
    window.FlashlsAPI.prototype.playerSetLogDebug = function (state) {
        this.flashObject.playerSetLogDebug(state);
    };
    window.FlashlsAPI.prototype.getLogDebug = function () {
        return this.flashObject.getLogDebug();
    };
    window.FlashlsAPI.prototype.playerSetLogDebug2 = function (state) {
        this.flashObject.playerSetLogDebug2(state);
    };
    window.FlashlsAPI.prototype.getLogDebug2 = function () {
        return this.flashObject.getLogDebug2();
    };
    window.FlashlsAPI.prototype.playerSetUseHardwareVideoDecoder = function (state) {
        this.flashObject.playerSetUseHardwareVideoDecoder(state);
    };
    window.FlashlsAPI.prototype.getUseHardwareVideoDecoder = function () {
        return this.flashObject.getUseHardwareVideoDecoder();
    };
    window.FlashlsAPI.prototype.playerSetflushLiveURLCache = function (state) {
        this.flashObject.playerSetflushLiveURLCache(state);
    };
    window.FlashlsAPI.prototype.getflushLiveURLCache = function () {
        return this.flashObject.getflushLiveURLCache();
    };
    
    window.FlashlsAPI.prototype.playerSetJSURLStream = function (state) {
        this.flashObject.playerSetJSURLStream(state);
    };
    
    window.FlashlsAPI.prototype.getJSURLStream = function () {
        return this.flashObject.getJSURLStream();
    };
    
    window.FlashlsAPI.prototype.playerCapLeveltoStage = function (state) {
        this.flashObject.playerCapLeveltoStage(state);
    };
    
    window.FlashlsAPI.prototype.getCapLeveltoStage = function () {
        return this.flashObject.getCapLeveltoStage();
    };
    
    window.FlashlsAPI.prototype.playerSetAutoLevelCapping = function (level) {
        this.flashObject.playerSetAutoLevelCapping(level);
    };
    
    window.FlashlsAPI.prototype.getAutoLevelCapping = function () {
        return this.flashObject.getAutoLevelCapping();
    };
})();

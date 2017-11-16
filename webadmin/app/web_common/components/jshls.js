'use strict';

function JsHlsAPI(){
    var events, stats, fmp4Data,
    debugMode, //Create the jshls player in debug mode
    enableWorker = true,
    //levelCapping = -1,
    dumpfMP4 = false,
    autoRecoverError = true;

    this.initHlsEvents = function(hls){
        var jshlsApi = this;
        hls.on(Hls.Events.MEDIA_ATTACHED,function() {
            events.video.push({time : performance.now() - events.t0, type : "Media attached"});
        });
        hls.on(Hls.Events.MEDIA_DETACHED,function() {
            events.video.push({time : performance.now() - events.t0, type : "Media detached"});
        });
        hls.on(Hls.Events.FRAG_PARSING_INIT_SEGMENT,function(event,data) {
            var event = {time : performance.now() - events.t0, type : data.id + " init segment"};
            events.video.push(event);
        });
        hls.on(Hls.Events.LEVEL_SWITCHING,function(event,data) {
            events.level.push({time : performance.now() - events.t0, id : data.level, bitrate : Math.round(hls.levels[data.level].bitrate/1000)});
        });
        hls.on(Hls.Events.MANIFEST_PARSED,function(event,data) {
            var event = {
                type : "manifest",
                name : "",
                start : 0,
                end : data.levels.length,
                time : data.stats.trequest - events.t0,
                latency : data.stats.tfirst - data.stats.trequest,
                load : data.stats.tload - data.stats.tfirst,
                duration : data.stats.tload - data.stats.tfirst,
            };

            events.load.push(event);
        });
        hls.on(Hls.Events.MANIFEST_PARSED,function(event,data) {
            stats = {levelNb: data.levels.length, levelParsed : 0};
        });
        hls.on(Hls.Events.AUDIO_TRACK_SWITCHING,function(event,data) {
            var event = {time : performance.now() - events.t0, type : 'audio switching', name : '@' + data.id };
            events.video.push(event);
            lastAudioTrackSwitchingIdx = events.video.length-1;
        });
        hls.on(Hls.Events.AUDIO_TRACK_SWITCHED,function(event,data) {
            var event = {time : performance.now() - events.t0, type : 'audio switched', name : '@' + data.id };
            if (lastAudioTrackSwitchingIdx !== undefined) {
                events.video[lastAudioTrackSwitchingIdx].duration = event.time - events.video[lastAudioTrackSwitchingIdx].time;
                lastAudioTrackSwitchingIdx = undefined;
            }
            events.video.push(event);
        });
        hls.on(Hls.Events.LEVEL_LOADED,function(event,data) {
            events.isLive = data.details.live;
            var event = {
                type : "level",
                id : data.level,
                start : data.details.startSN,
                end : data.details.endSN,
                time : data.stats.trequest - events.t0,
                latency : data.stats.tfirst - data.stats.trequest,
                load : data.stats.tload - data.stats.tfirst,
                parsing : data.stats.tparsed - data.stats.tload,
                duration : data.stats.tload - data.stats.tfirst
            };
            var parsingDuration = data.stats.tparsed - data.stats.tload;
            if (stats.levelParsed) {
                this.sumLevelParsingMs += parsingDuration;
            }
            else {
                this.sumLevelParsingMs = parsingDuration;
            }
            stats.levelParsed++;
            stats.levelParsingUs = Math.round(1000*this.sumLevelParsingMs / stats.levelParsed);
            //console.log("parsing level duration :" + stats.levelParsingUs + "us,count:" + stats.levelParsed);
            events.load.push(event);
        });
        hls.on(Hls.Events.AUDIO_TRACK_LOADED,function(event,data) {
            events.isLive = data.details.live;
            var event = {
                type : "audio track",
                id : data.id,
                start : data.details.startSN,
                end : data.details.endSN,
                time : data.stats.trequest - events.t0,
                latency : data.stats.tfirst - data.stats.trequest,
                load : data.stats.tload - data.stats.tfirst,
                parsing : data.stats.tparsed - data.stats.tload,
                duration : data.stats.tload - data.stats.tfirst
            };
            events.load.push(event);
        });
        hls.on(Hls.Events.FRAG_BUFFERED,function(event,data) {
            var event = {
                type : data.frag.type + " fragment",
                id : data.frag.level,
                id2 : data.frag.sn,
                time : data.stats.trequest - events.t0,
                latency : data.stats.tfirst - data.stats.trequest,
                load : data.stats.tload - data.stats.tfirst,
                parsing : data.stats.tparsed - data.stats.tload,
                buffer : data.stats.tbuffered - data.stats.tparsed,
                duration : data.stats.tbuffered - data.stats.tfirst,
                bw : Math.round(8*data.stats.total/(data.stats.tbuffered - data.stats.trequest)),
                size : data.stats.total
            };
            events.load.push(event);
            events.bitrate.push({time : performance.now() - events.t0, bitrate : event.bw , duration : data.frag.duration, level : event.id});

            var latency = data.stats.tfirst - data.stats.trequest,
            parsing = data.stats.tparsed - data.stats.tload,
            process = data.stats.tbuffered - data.stats.trequest,
            bitrate = Math.round(8 * data.stats.length / (data.stats.tbuffered - data.stats.tfirst));
            if (stats.fragBuffered) {
                stats.fragMinLatency = Math.min(stats.fragMinLatency, latency);
                stats.fragMaxLatency = Math.max(stats.fragMaxLatency, latency);
                stats.fragMinProcess = Math.min(stats.fragMinProcess, process);
                stats.fragMaxProcess = Math.max(stats.fragMaxProcess, process);
                stats.fragMinKbps = Math.min(stats.fragMinKbps, bitrate);
                stats.fragMaxKbps = Math.max(stats.fragMaxKbps, bitrate);
                stats.autoLevelCappingMin = Math.min(stats.autoLevelCappingMin, hls.autoLevelCapping);
                stats.autoLevelCappingMax = Math.max(stats.autoLevelCappingMax, hls.autoLevelCapping);
                stats.fragBuffered++;
            }
            else {
                stats.fragMinLatency = stats.fragMaxLatency = latency;
                stats.fragMinProcess = stats.fragMaxProcess = process;
                stats.fragMinKbps = stats.fragMaxKbps = bitrate;
                stats.fragBuffered = 1;
                stats.fragBufferedBytes = 0;
                stats.autoLevelCappingMin = stats.autoLevelCappingMax = hls.autoLevelCapping;
                this.sumLatency = 0;
                this.sumKbps = 0;
                this.sumProcess = 0;
                this.sumParsing = 0;
            }
            stats.fraglastLatency = latency;
            this.sumLatency += latency;
            stats.fragAvgLatency = Math.round(this.sumLatency / stats.fragBuffered);
            stats.fragLastProcess = process;
            this.sumProcess += process;
            this.sumParsing += parsing;
            stats.fragAvgProcess = Math.round(this.sumProcess / stats.fragBuffered);
            stats.fragLastKbps = bitrate;
            this.sumKbps += bitrate;
            stats.fragAvgKbps = Math.round(this.sumKbps / stats.fragBuffered);
            stats.fragBufferedBytes += data.stats.total;
            stats.fragparsingKbps = Math.round(8*stats.fragBufferedBytes / this.sumParsing);
            stats.fragparsingMs = Math.round(this.sumParsing);
            stats.autoLevelCappingLast = hls.autoLevelCapping;
        });
        hls.on(Hls.Events.LEVEL_SWITCHED,function(event,data) {
            var event = {time : performance.now() - events.t0, type : 'level switched', name : data.level };
            events.video.push(event);
        });
        hls.on(Hls.Events.FRAG_CHANGED,function(event,data) {
            var event = {time : performance.now() - events.t0, type : 'frag changed', name : data.frag.sn + ' @ ' + data.frag.level };
            events.video.push(event);
            stats.tagList = data.frag.tagList;

            var level = data.frag.level, autoLevel = data.frag.autoLevel;
            if (stats.levelStart === undefined) {
                stats.levelStart = level;
            }
            if (autoLevel) {
                if (stats.fragChangedAuto) {
                    stats.autoLevelMin = Math.min(stats.autoLevelMin, level);
                    stats.autoLevelMax = Math.max(stats.autoLevelMax, level);
                    stats.fragChangedAuto++;
                    if (this.levelLastAuto && level !== stats.autoLevelLast) {
                        stats.autoLevelSwitch++;
                    }
                }
                else {
                    stats.autoLevelMin = stats.autoLevelMax = level;
                    stats.autoLevelSwitch = 0;
                    stats.fragChangedAuto = 1;
                    this.sumAutoLevel = 0;
                }
                this.sumAutoLevel += level;
                stats.autoLevelAvg = Math.round(1000 * this.sumAutoLevel / stats.fragChangedAuto) / 1000;
                stats.autoLevelLast = level;
            }
            else {
                if (stats.fragChangedManual) {
                    stats.manualLevelMin = Math.min(stats.manualLevelMin, level);
                    stats.manualLevelMax = Math.max(stats.manualLevelMax, level);
                    stats.fragChangedManual++;
                    if (!this.levelLastAuto && level !== stats.manualLevelLast) {
                    stats.manualLevelSwitch++;
                    }
                }
                else {
                    stats.manualLevelMin = stats.manualLevelMax = level;
                    stats.manualLevelSwitch = 0;
                    stats.fragChangedManual = 1;
                }
                stats.manualLevelLast = level;
            }

            this.levelLastAuto = autoLevel;
        });
        hls.on(Hls.Events.FRAG_LOAD_EMERGENCY_ABORTED,function(event,data) {
            if (stats) {
                if (stats.fragLoadEmergencyAborted === undefined) {
                    stats.fragLoadEmergencyAborted = 1;
                }
                else {
                    stats.fragLoadEmergencyAborted++;
                }
            }
        });
        hls.on(Hls.Events.FRAG_DECRYPTED,function(event,data) {
            if (!stats.fragDecrypted) {
                stats.fragDecrypted = 0;
                this.totalDecryptTime = 0;
                stats.fragAvgDecryptTime = 0;
            }
            stats.fragDecrypted++;
            this.totalDecryptTime += data.stats.tdecrypt - data.stats.tstart;
            stats.fragAvgDecryptTime = this.totalDecryptTime / stats.fragDecrypted;
        });
        hls.on(Hls.Events.ERROR, function(event,data) {
            console.warn(data);

            switch(data.details) {
                case Hls.ErrorDetails.MANIFEST_LOAD_ERROR:
                    try {
                        console.log("cannot Load <a href=\"" + data.context.url + "\">" + data.context.url + "</a><br>HTTP response code:" + data.response.code + " <br>" + data.response.text);
                        if(data.response.code === 0) {
                            console.log("this might be a CORS issue, consider installing <a href=\"https://chrome.google.com/webstore/detail/allow-control-allow-origi/nlfbmbojpeacfghkpbjhddihlkkiljbi\">Allow-Control-Allow-Origin</a> Chrome Extension");
                        }
                    }
                    catch(err) {
                        console.log("cannot Load <a href=\"" + data.context.url + "\">" + data.context.url + "</a><br>Reason:Load " + data.response.text);
                    }
                    break;
                case Hls.ErrorDetails.MANIFEST_LOAD_TIMEOUT:
                    console.log("timeout while loading manifest");
                    break;
                case Hls.ErrorDetails.MANIFEST_PARSING_ERROR:
                    console.log("error while parsing manifest:" + data.reason);
                    break;
                case Hls.ErrorDetails.LEVEL_LOAD_ERROR:
                    console.log("error while loading level playlist");
                    break;
                case Hls.ErrorDetails.LEVEL_LOAD_TIMEOUT:
                    console.log("timeout while loading level playlist");
                    break;
                case Hls.ErrorDetails.LEVEL_SWITCH_ERROR:
                    console.log("error while trying to switch to level " + data.level);
                    break;
                case Hls.ErrorDetails.FRAG_LOAD_ERROR:
                    console.log("error while loading fragment " + data.frag.url);
                    break;
                case Hls.ErrorDetails.FRAG_LOAD_TIMEOUT:
                    console.log("timeout while loading fragment " + data.frag.url);
                    break;
                case Hls.ErrorDetails.FRAG_LOOP_LOADING_ERROR:
                    console.log("Frag Loop Loading Error");
                    break;
                case Hls.ErrorDetails.FRAG_DECRYPT_ERROR:
                    console.log("Decrypting Error:" + data.reason);
                    break;
                case Hls.ErrorDetails.FRAG_PARSING_ERROR:
                    console.log("Parsing Error:" + data.reason);
                    break;
                case Hls.ErrorDetails.KEY_LOAD_ERROR:
                    console.log("error while loading key " + data.frag.decryptdata.uri);
                    break;
                case Hls.ErrorDetails.KEY_LOAD_TIMEOUT:
                    console.log("timeout while loading key " + data.frag.decryptdata.uri);
                    break;
                case Hls.ErrorDetails.BUFFER_APPEND_ERROR:
                    console.log("Buffer Append Error");
                    break;
                case Hls.ErrorDetails.BUFFER_ADD_CODEC_ERROR:
                    console.log("Buffer Add Codec Error for " + data.mimeType + ":" + data.err.message);
                    break;
                case Hls.ErrorDetails.BUFFER_APPENDING_ERROR:
                    console.log("Buffer Appending Error");
                    break;
                /*case Hls.ErrorDetails.BUFFER_STALLED_ERROR:
                    console.log("Buffer Stalled Error");
                    console.log(jshlsApi.hls.streamController._bufferedFrags);
                    //jshlsApi.hls.handleMediaError();*/
                default:
                    break;
            }
            if(data.fatal) {
                console.log('fatal error :' + data.details);
                switch(data.type) {
                    case Hls.ErrorTypes.MEDIA_ERROR:
                        jshlsApi.handleMediaError();
                        break;
                    case Hls.ErrorTypes.NETWORK_ERROR:
                        console.log(",network error ...");
                        break;
                    default:
                        console.log(", unrecoverable error");
                        hls.destroy();
                        break;
                }
                jshlsApi.errorHandler("Fatal Error");
            }
            if(!stats) stats = {};
            // track all errors independently
            if (stats[data.details] === undefined) {
                stats[data.details] = 1;
            }
            else {
                stats[data.details] += 1;
            }
            // track fatal error
            if (data.fatal) {
                if (stats.fatalError === undefined) {
                    stats.fatalError = 1;
                }
                else {
                    stats.fatalError += 1;
                }
            }
        });
        hls.on(Hls.Events.BUFFER_CREATED, function(event,data) {
            tracks = data.tracks;
        });
        hls.on(Hls.Events.BUFFER_APPENDING, function(event,data) {
            if (dumpfMP4) {
                fmp4Data[data.type].push(data.data);
            }
        });
        hls.on(Hls.Events.FPS_DROP,function(event,data) {
            var evt = {time : performance.now() - events.t0, type : "frame drop", name : data.currentDropped + "/" + data.currentDecoded};
            events.video.push(evt);
            if (stats) {
                if (stats.fpsDropEvent === undefined) {
                    stats.fpsDropEvent = 1;
                }
                else {
                    stats.fpsDropEvent++;
                }
                stats.fpsTotalDroppedFrames = data.totalDroppedFrames;
            }
        });
    };

    this.init = function(element, loadingTimeOut, jshlsDebugMode, readyHandler, errorHandler){
        this.video = element[0];
        
        debugMode = jshlsDebugMode;
        if(Hls.isSupported()) {
            if(this.hls) {
                this.hls.destroy();
                this.hls = null;
            }
        }
        events = { url : '', t0 : performance.now(), load : [], buffer : [], video : [], level : [], bitrate : []};
        recoverDecodingErrorDate = recoverSwapAudioCodecDate = null;
        fmp4Data = { 'audio': [], 'video': [] };

        this.hls = new Hls({
            debug: debugMode,
            enableWorker: enableWorker,
            manifestLoadingTimeOut: loadingTimeOut,
            levelLoadingTimeOut: loadingTimeOut, // used by playlist-loader
            fragLoadingTimeOut: loadingTimeOut
        });

        this.initHlsEvents(this.hls);        
        this.initVideoHandlers();
        this.errorHandler = errorHandler;
        this.readyHandler = readyHandler;
        this.readyHandler(this);
    };

    var lastSeekingIdx, lastStartPosition,lastDuration, lastAudioTrackSwitchingIdx;
    this.handleVideoEvent= function(evt){
        var data = '';
        switch(evt.type) {
            case 'durationchange':
                if(evt.target.duration - lastDuration <= 0.5) {
                // some browsers reports several duration change events with almost the same value ... avoid spamming video events
                return;
                }
                lastDuration = evt.target.duration;
                data = Math.round(evt.target.duration*1000);
                break;
            case 'resize':
                data = evt.target.videoWidth + '/' + evt.target.videoHeight;
                break;
            case 'loadedmetadata':
            case 'loadeddata':
            case 'canplay':
            case 'canplaythrough':
            case 'ended':
            case 'seeking':
            case 'seeked':
            case 'play':
            case 'playing':
                lastStartPosition = evt.target.currentTime;
            case 'pause':
            case 'waiting':
            case 'stalled':
            case 'error':
                data = Math.round(evt.target.currentTime*1000);
                if(evt.type === 'error') {
                var errorTxt,mediaError=evt.currentTarget.error;
                switch(mediaError.code) {
                    case mediaError.MEDIA_ERR_ABORTED:
                    errorTxt = "You aborted the video playback";
                    break;
                case mediaError.MEDIA_ERR_DECODE:
                    errorTxt = "The video playback was aborted due to a corruption problem or because the video used features your browser did not support";
                    this.handleMediaError();
                    break;
                case mediaError.MEDIA_ERR_NETWORK:
                    errorTxt = "A network error caused the video download to fail part-way";
                    break;
                case mediaError.MEDIA_ERR_SRC_NOT_SUPPORTED:
                    errorTxt = "The video could not be loaded, either because the server or network failed or because the format is not supported";
                    break;
                }
                console.error(errorTxt);
                }
                break;
            default:
                break;
        }
        var event = {time : performance.now() - evt.t0, type : evt.type, name : data};
        events.video.push(event);
        if(evt.type === 'seeking') {
            lastSeekingIdx = events.video.length-1;
        }
        if(evt.type === 'seeked') {
            events.video[lastSeekingIdx].duration = event.time - events.video[lastSeekingIdx].time;
        }
    };

    var recoverDecodingErrorDate,recoverSwapAudioCodecDate;
    this.handleMediaError = function(){
        if(autoRecoverError) {
            var now = performance.now();
            if(!recoverDecodingErrorDate || (now - recoverDecodingErrorDate) > 3000) {
                recoverDecodingErrorDate = performance.now();
                console.log(",try to recover media Error ...");
                this.hls.recoverMediaError();
            } else {
                if(!recoverSwapAudioCodecDate || (now - recoverSwapAudioCodecDate) > 3000) {
                    recoverSwapAudioCodecDate = performance.now();
                    console.log(",try to swap Audio Codec and recover media Error ...");
                    this.hls.swapAudioCodec();
                    this.hls.recoverMediaError();
                } else {
                    console.log(",cannot recover, last media error recovery failed ...");
                }
            }
        }
    };
}

JsHlsAPI.prototype.kill = function(){
    this.hls.destroy();
};

JsHlsAPI.prototype.play = function(offset){
    this.video.play().catch(function(error){
        var errorString = error.toString();
        if(errorString.indexOf('pause') < 0 && errorString.indexOf('load') < 0){
            throw error;
        }
    });
};

JsHlsAPI.prototype.pause = function(){
    this.video.pause();
};

JsHlsAPI.prototype.volume = function(volumeLevel){
    this.video.volume = volumeLevel/100;
};

JsHlsAPI.prototype.load = function(url){
    this.hls.detachMedia();
    this.hls.loadSource(url);
    this.hls.autoLevelCapping = -1; //levelCapping;
    this.hls.attachMedia(this.video);
};

JsHlsAPI.prototype.initVideoHandlers = function(){
    this.addEventListener('resize', this.handleVideoEvent);
    this.addEventListener('seeking', this.handleVideoEvent);
    this.addEventListener('seeked', this.handleVideoEvent);
    this.addEventListener('pause', this.handleVideoEvent);
    this.addEventListener('play', this.handleVideoEvent);
    this.addEventListener('canplay', this.handleVideoEvent);
    this.addEventListener('canplaythrough', this.handleVideoEvent);
    this.addEventListener('ended', this.handleVideoEvent);
    this.addEventListener('playing', this.handleVideoEvent);
    this.addEventListener('error', this.handleVideoEvent);
    this.addEventListener('loadedmetadata', this.handleVideoEvent);
    this.addEventListener('loadeddata', this.handleVideoEvent);
    this.addEventListener('durationchange', this.handleVideoEvent);
};

JsHlsAPI.prototype.addEventListener = function(event, handler){
    this.handlers = this.handlers || {};
    if(this.video && this.handlers[event] !== handler){
        this.video.addEventListener(event,handler);
        this.handlers[event] = handler;
    }
};

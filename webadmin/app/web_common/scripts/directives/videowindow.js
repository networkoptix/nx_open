'use strict';

/**
 * This is smart video-plugin.
 * 1. It gets list of possible video-sources in different formats (each requires mime-type)
 * (Outer code decides, if some video-format is supported by exact server)
 * 2. Detects browser and operating system
 * 3. Chooses best possible video-source for this browser
 * 4. If there is now such format - he tries to detect possible plugin for rtsp and use it
 *
 * This plugin hides details from outer code either.
 *
 * API:
 * update playing time handler
 * change source
 * play/pause
 * seekPosition - later
 * playingSpeed - later
 *
 */
var flashPlayer = "";

angular.module('nxCommon')
    .directive('videowindow', ['$interval','$timeout','animateScope', '$sce', '$log', function ($interval,$timeout,animateScope, $sce, $log) {
        return {
            restrict: 'E',
            scope: {
                vgUpdateTime:"&",
                vgPlayerReady:"&",
                playerId: "=",
                vgSrc:"=",
                player:"=",
                activeFormat:"=",
                rotation: "=",
                playerHandler: "="
            },
            templateUrl: Config.viewsDirCommon + 'components/videowindow.html',// ???

            link: function (scope, element/*, attrs*/) {
                var mimeTypes = {
                    'hls': 'application/x-mpegURL',
                    'webm': 'video/webm',
                    'rtsp': 'application/x-rtsp',
                    'flv': 'video/x-flv',
                    'mp4': 'video/mp4',
                    'jpeg': 'image/jpeg'
                };
                scope.Config = Config;
                scope.debugMode = Config.allowDebugMode;
                scope.videoFlags = {};
                scope.loading = false; // Initiate state - not loading (do nothing)
                
                function getFormatSrc(mediaformat) {
                    var src = _.find(scope.vgSrc,function(src){return src.type == mimeTypes[mediaformat];});
                    if( scope.debugMode){
                        console.log("playing",src?src.src:null);
                    }
                    //return "http://sample.vodobox.net/skate_phantom_flex_4k/skate_phantom_flex_4k.m3u8";
                    //return "https://bitdash-a.akamaihd.net/content/sintel/hls/playlist.m3u8";
                    //return "http://184.72.239.149/vod/smil:BigBuckBunny.smil/playlist.m3u8";
                    return src?src.src:null;
                }

                function detectBestFormat(){
                    //1. Hide all informers
                    scope.videoFlags = {
                        flashRequired: false,
                        flashOrWebmRequired: false,
                        noArmSupport: false,
                        noFormat: false,
                        errorLoading: false,
                        ieNoWebm: false,
                        ieWin10: false,                    
                        ubuntuNX: false
                    };

                    if(scope.debugMode && scope.activeFormat != "Auto"){
                        return scope.activeFormat;
                    }

                    //This function gets available sources for camera and chooses the best player for this browser

                    //return "rtsp"; // To debug some format - force it to work

                    //We have options:
                    // webm - for good desktop browsers
                    // webm-codec - for IE. Detectf
                    // native-hls - for mobile browsers
                    // flv - for desktop browsers - not supported yet
                    // rtsp - for desktop browsers
                    // activex-rtsp - for some browsers - not supported yet
                    // flashls - for desktop browsers
                    // jshls - for desktop browsers - not supported yet

                    function canPlayNatively(type){
                        var res = false;
                        var v = document.createElement('video');
                        if(v.canPlayType && v.canPlayType(mimeTypes[type]).replace(/no/, '')) {
                            res = true; // we have webm codec!
                        }
                        return res;
                    }
                    var weHaveWebm = _.find(scope.vgSrc,function(src){return src.type == mimeTypes['webm'];});
                    var weHaveHls = _.find(scope.vgSrc,function(src){return src.type == mimeTypes['hls'];});
                    var jsHlsSupported = Hls.isSupported();

                    //Should Catch MS edge, Safari, Mobile Devices
                    if(weHaveHls && (canPlayNatively("hls") || window.jscd.mobile)){
                        return "native-hls";
                    }

                    // Hardcode native support
                    if(window.jscd.os == "Android" ){
                        if(weHaveWebm){
                            return "webm";
                            // TODO: Try removing this line.
                        }else {
                            scope.videoFlags.noArmSupport = true;
                            return false;
                        }
                    }

                    // No native support
                    //Presume we are on desktop:
                    switch(window.jscd.browser){
                        case 'Microsoft Internet Explorer':
                            // Check version here
                            if(jsHlsSupported && weHaveHls){
                                return "jshls";
                            }
                            if(window.jscd.flashVersion && weHaveHls){ // We have flash - try to play using flash
                                return "flashls";
                            }
                            if(weHaveWebm && window.jscd.osVersion < 10 && canPlayNatively("webm")){
                                return 'webm';
                            }
                            //Could not find a supported player for the Browser gonna display whats needed instead.
                            if(weHaveWebm){
                                if(window.jscd.osVersion < 10){
                                    if(weHaveHls){
                                        scope.videoFlags.flashOrWebmRequired = true;
                                    }
                                    else{
                                        scope.videoFlags.ieNoWebm = true;
                                    }
                                }
                                else{
                                    scope.videoFlags.ieWin10 = true;
                                }
                                return false;
                            }
                            break;

                        case "Firefox":
                        case "Chrome":
                        case "Opera":
                        case "Webkit":
                        default:
                            if(jsHlsSupported && weHaveHls) {
                                return "jshls";// We are hoping that we have some good browser
                            }
                            if(window.jscd.flashVersion &&  weHaveHls){ // We have flash - try to play using flash
                                return "flashls";
                            }
                            if(weHaveWebm && canPlayNatively("webm")){
                                return "webm";
                            }
                            if(weHaveHls && window.jscd.os === 'Linux'){
                                scope.videoFlags.ubuntuNX = true;
                                return false;
                            }
                    }

                    if(weHaveHls){
                        scope.videoFlags.flashRequired = true;
                    }
                    else{
                        scope.videoFlags.noFormat = true;
                    }
                    return false; // IE9 - No supported formats
                }

                // TODO: Create common interface for each player, html5 compatible or something
                // TODO: move supported info to config
                // TODO: Support new players

                var makingPlayer = false;
                var nativePlayerLoadError = null;

                //For the native player. Handles webm's long loading times
                function loadingTimeout(){
                    scope.videoFlags.errorLoading = true;
                    scope.loading = false;
                    nativePlayerLoadError = null;
                    resetPlayer();
                }

                function cancelTimeoutNativeLoad(){
                    if(nativePlayerLoadError){
                        $timeout.cancel(nativePlayerLoadError);
                        nativePlayerLoadError = null;
                    }
                }

                function resetTimeout(event){
                    if(nativePlayerLoadError){
                        scope.loading = true;
                        cancelTimeoutNativeLoad();
                        nativePlayerLoadError = $timeout(loadingTimeout, Config.webclient.nativeTimeout);
                    }
                }

                function initNativePlayer(nativeFormat) {

                    scope.native = true;
                    scope.flashls = false;
                    scope.jsHls = false;

                    $timeout(function(){
                        var nativePlayer = new NativePlayer();
                        nativePlayer.init(element.find(".videoplayer"), function (api) {
                            makingPlayer = false;
                            scope.vgApi = api;
                            cancelTimeoutNativeLoad();

                            if (scope.vgSrc) {
                                scope.vgApi.load(getFormatSrc(nativeFormat), mimeTypes[nativeFormat]);

                                scope.vgApi.addEventListener("timeupdate", function (event) {
                                    var video = event.srcElement || event.originalTarget;
                                    scope.vgUpdateTime({$currentTime: video.currentTime, $duration: video.duration});
                                });

                                scope.vgApi.addEventListener("loadeddata", function(event){
                                    scope.loading = false; // Video is playing - disable loading
                                    scope.playerHandler();
                                    cancelTimeoutNativeLoad();
                                });

                                scope.vgApi.addEventListener("ended",function(event){
                                    scope.vgUpdateTime({$currentTime: null, $duration: null});
                                });

                                scope.vgApi.addEventListener("loadstart", function (event){
                                    if(nativePlayerLoadError){
                                        $timeout.cancel(nativePlayerLoadError);
                                    }
                                    nativePlayerLoadError = $timeout(loadingTimeout, Config.webclient.nativeTimeout);
                                });

                                //If we are still downloading the video reset the timer
                                scope.vgApi.addEventListener("progress", resetTimeout);

                                //If the player stalls give it a chance to recover
                                scope.vgApi.addEventListener("stalled", resetTimeout);
                            }

                            scope.vgPlayerReady({$API: scope.vgApi});
                        });
                    });
                }

                function initFlashls() {
                    scope.flashls = true;
                    scope.native = false;
                    scope.jsHls = false;

                    var playerId = scope.playerId;
                    if(!playerId){
                        playerId = "player0";
                    }
                    
                    scope.flashSource = Config.webclient.staticResources + Config.webclient.flashChromelessPath;
                    if(scope.debugMode){
                        scope.flashSource = Config.webclient.staticResources + Config.webclient.flashChromelessDebugPath;
                    }

                    var flashlsAPI = new FlashlsAPI(null);

                    if(flashlsAPI.ready()){
                        flashlsAPI.kill();
                        scope.flashls = false; // Destroy it!
                        $timeout(initFlashls);
                    }else {
                        $timeout(function () {// Force DOM to refresh here
                            flashlsAPI.init(playerId, function (api) {
                                makingPlayer = false;
                                scope.vgApi = api;
                                if (scope.vgSrc) {
                                    scope.vgApi.load(getFormatSrc('hls'));
                                }
                                scope.vgPlayerReady({$API: api});
                            },
                            playerErrorHandler,
                            function (position, duration) {
                                scope.loading = false; // Video is playing - disable loading
                                scope.vgUpdateTime({$currentTime: position, $duration: duration});
                            });
                        });
                    }
                }

                function initJsHls(){
                    scope.flashls = false;
                    scope.native = false;
                    scope.jsHls = true;

                    $timeout(function(){
                        var jsHlsAPI = new JsHlsAPI();
                        jsHlsAPI.init(element.find(".videoplayer"),
                            Config.webclient.hlsLoadingTimeout,
                            scope.debugMode,
                            function (api) {
                                makingPlayer = false;
                                scope.vgApi = api;
                                if (scope.vgSrc) {
                                    scope.vgApi.load(getFormatSrc('hls'));

                                    scope.vgApi.addEventListener("loadeddata", function(){
                                        scope.loading = false;  // Video is ready - disable loading
                                        scope.playerHandler();
                                    });

                                    scope.vgApi.addEventListener("timeupdate", function (event) {
                                        var video = event.srcElement || event.originalTarget;
                                        scope.vgUpdateTime({$currentTime: video.currentTime, $duration: video.duration});
                                    });

                                    scope.vgApi.addEventListener("ended",function(event){
                                        scope.vgUpdateTime({$currentTime: null, $duration: null});
                                    });
                                }
                                scope.vgPlayerReady({$API:api});
                            }, playerErrorHandler);
                    });
                }

                element.bind('contextmenu',function() { return !!scope.debugMode; }); // Kill context menu
                
                function playerErrorHandler(error){
                    scope.videoFlags.errorLoading = true;
                    scope.loading = false; // Some error happended - stop loading
                    resetPlayer();
                    scope.playerHandler(error);
                    console.error(error);
                }

                function initNewPlayer(){
                    if( makingPlayer ){
                        return;
                    }
                    makingPlayer = true;
                    switch(scope.player){
                        case "flashls":
                            initFlashls();
                            break;

                        case "jshls":
                            initJsHls();
                            break;

                        case "native-hls":
                            initNativePlayer("hls");
                            break;

                        case "webm":
                        default:
                            initNativePlayer(scope.player);
                            break;
                    }
                }

                function resetPlayer(){
                    if(scope.vgApi){
                        scope.vgApi.kill();
                        makingPlayer = false;
                        scope.vgApi = null;
                    }
                    scope.vgPlayerReady({$API: null});
                    //Turn off all players to reset ng-class for rotation
                    scope.native = false;
                    scope.flashls = false;
                    scope.jsHls = false;
                }

                function srcChanged(){
                    scope.loading = true; // source changed - start loading
                    scope.videoFlags.errorLoading = false;
                    if(scope.vgSrc ) {
                        scope.preview = getFormatSrc('jpeg');
                        scope.player = detectBestFormat();
                        resetPlayer();

                        if(!scope.player){
                            scope.loading = false; // no supported format - no loading
                            return;
                        }
                        $timeout(initNewPlayer);
                        $timeout(updateWidth);
                    }
                }

                scope.$watch("vgSrc",srcChanged, true);

                scope.$on('$destroy',function(){
                    resetPlayer();
                });

                if(scope.debugMode){
                    scope.$watch('activeFormat', srcChanged);
                }
                
                scope.initFlash = function(){
                    var playerId = !scope.playerId ? 'player0': scope.playerId;
                    
                    if(!flashPlayer){
                        $.ajax({
                            url: Config.viewsDirCommon + 'components/flashPlayer.html',
                            success: function(result){
                                flashPlayer = result;
                                flashPlayer = flashPlayer.replace(/{{playerId}}/g, playerId);
                                flashPlayer = flashPlayer.replace(/{{flashSource}}/g, scope.flashSource);
                                scope.flashPlayer = $sce.trustAsHtml(flashPlayer);
                            },
                            error: function(){
                                $log.error("There was a problem with loading the flash player!!!");
                            }
                        });
                    }
                    else{
                        scope.flashPlayer = $sce.trustAsHtml(flashPlayer);
                    }
                };

                scope.getRotation = function(){
                    if(scope.player == "webm"){
                        return "";
                    }
                    switch(scope.rotation){
                        case 90:
                            return "rotate90";
                        case 180:
                            return "rotate180";
                        case 270:
                            return "rotate270";
                        default:
                            return "";
                    }
                };

                var $videowindow = $('.videowindow');
                var $window = $(window);

                function updateWidth(){
                    if(!scope.rotation || scope.rotation == 0 || scope.rotation == 180){
                        return;
                    }
                    var videoWindowHeight = $videowindow.height();
                    $('.videoplayer').css('width',videoWindowHeight);
                }
                $window.resize(updateWidth);
            }
        }
    }]);

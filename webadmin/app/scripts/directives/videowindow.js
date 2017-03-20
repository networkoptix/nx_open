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
angular.module('webadminApp')
    .directive('videowindow', ['$interval','$timeout','animateScope', function ($interval,$timeout,animateScope) {
        return {
            restrict: 'E',
            scope: {
                vgUpdateTime:"&",
                vgPlayerReady:"&",
                vgSrc:"=",
                player:"="
            },
            templateUrl: Config.viewsDir + 'components/videowindow.html',// ???

            link: function (scope, element/*, attrs*/) {

                var mimeTypes = {
                    'hls': 'application/x-mpegURL',
                    'webm': 'video/webm',
                    'rtsp': 'application/x-rtsp',
                    'flv': 'video/x-flv',
                    'mp4': 'video/mp4'
                };
                scope.Config = Config;
                scope.debugMode = Config.debug.video && Config.allowDebugMode;
                scope.debugFormat = Config.allowDebugMode && Config.debug.videoFormat;

                function getFormatSrc(mediaformat) {
                    var src = _.find(scope.vgSrc,function(src){return src.type == mimeTypes[mediaformat];});
                    if( scope.debugMode){
                        console.log("playing",src?src.src:null);
                    }
                    return src?src.src:null;
                }

                function detectBestFormat(){
                    //1. Hide all informers
                    scope.flashRequired = false;
                    scope.flashOrWebmRequired = false;
                    scope.noArmSupport = false;
                    scope.noFormat = false;
                    scope.errorLoading = false;
                    scope.ieNoWebm = false;
                    scope.loading = false;
                    scope.ieWin10 = false;
                    scope.ubuntuNX = false;

                    if(scope.debugMode && scope.debugFormat){
                        return scope.debugFormat;
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

                    //Should Catch MS edge, Safari, Mobile Devic
                    if(weHaveHls && (canPlayNatively("hls") || window.jscd.mobile)){
                        return "native-hls";
                    }

                    // Hardcode native support
                    if(window.jscd.os == "Android" ){
                        if(weHaveWebm){
                            return "webm";
                            // TODO: Try removing this line.
                        }else {
                            scope.noArmSupport = true;
                            return false;
                        }
                    }

                    // No native support
                    //Presume we are on desktop:
                    switch(window.jscd.browser){
                        case 'Microsoft Internet Explorer':
                            // Check version here
                            if(window.jscd.browserMajorVersion>=10 && weHaveHls){
                                return "jshls";
                            }
                            if(window.jscd.flashVersion){ // We have flash - try to play using flash
                                return "flashls";
                            }
                            if (weHaveWebm){
                                return 'webm';
                            }
                            //Could not find a supported player for the Browser gonna display whats needed instead.
                            if(!weHaveWebm){
                                if(window.jscd.osVersion < 10){
                                    if(!weHaveHls){
                                        scope.flashOrWebmRequired = true;
                                    }
                                    else{
                                        scope.ieNoWebm = true;
                                    }
                                }
                                else{
                                    scope.ieWin10 = true;
                                }
                            }
                            break;

                        case "Firefox":
                            if(weHaveHls && window.jscd.os === 'Linux'){
                                scope.ubuntuNX = true;
                                return false;
                            }

                        case "Chrome":
                        case "Opera":
                        case "Webkit":
                        default:
                            if(weHaveHls) {
                                return "jshls";// We are hoping that we have some good browser
                            }
                            if(window.jscd.flashVersion){ // We have flash - try to play using flash
                                return "flashls";
                            }
                            if(weHaveWebm){
                                return "webm";
                            }
                    }

                    scope.flashRequired = true;
                    scope.noFormat = true;
                    return false; // IE9 - No supported formats
                }


                //TODO: remove ID, generate it dynamically

                var activePlayer = null;
                function newPlayer(player){
                    if(activePlayer != player) {
                        element.find(".videoplayer").html("");
                        scope.vgPlayerReady({$API: null});
                        if(activePlayer == 'flashls' && player == 'webm'){
                            // This is hack! When switching from flashls to webm video - webm player stucks. Seems like chrome issue.
                            activePlayer = player;
                            return true;
                        }
                        activePlayer = player;
                    }
                    return false;
                }

                // TODO: Create common interface for each player, html5 compatible or something
                // TODO: move supported info to config
                // TODO: Support new players

                function initNativePlayer(nativeFormat) {

                    scope.native = true;
                    scope.flashls = false;
                    scope.jsHls = false;

                    var autoshow = null;
                    nativePlayer.init(element.find(".videoplayer"), function (api) {
                        scope.vgApi = api;

                        if (scope.vgSrc) {
                            $timeout(function () {
                                scope.loading = !!format;
                            });

                            if(format == 'webm' && window.jscd.os == "Android" ){ // TODO: this is hach for android bug. remove it later
                                if(autoshow){
                                    $timeout.cancel(autoshow);
                                }
                                autoshow = $timeout(function () {
                                    scope.loading = false;
                                    autoshow = null;
                                },20000);
                            }

                            scope.vgApi.load(getFormatSrc(nativeFormat), mimeTypes[nativeFormat]);

                            scope.vgApi.addEventListener("timeupdate", function (event) {
                                var video = event.srcElement || event.originalTarget;
                                scope.vgUpdateTime({$currentTime: video.currentTime, $duration: video.duration});
                                if (scope.loading) {
                                    $timeout(function () {
                                        scope.loading = false;
                                    });
                                }
                            });

                            scope.vgApi.addEventListener("ended",function(event){
                                scope.vgUpdateTime({$currentTime: null, $duration: null});

                            });
                        }

                        scope.vgPlayerReady({$API: scope.vgApi});
                    }, function (api) {
                        console.error("some error");
                    });
                }

                function initFlashls() {
                    scope.flashls = true;
                    scope.native = false;
                    scope.jsHls = false;
                    scope.flashSource = "components/flashlsChromeless.swf";

                    if(scope.debugMode && scope.debugFormat){
                        scope.flashSource = "components/flashlsChromeless_debug.swf";
                    }

                    if(flashlsAPI.ready()){
                        flashlsAPI.kill();
                        scope.flashls = false; // Destroy it!
                        $timeout(initFlashls);
                    }else {
                        $timeout(function () {// Force DOM to refresh here
                            flashlsAPI.init("videowindow", function (api) {
                                scope.vgApi = api;
                                if (scope.vgSrc) {
                                    $timeout(function () {
                                        scope.loading = !!format;
                                    });
                                    scope.vgApi.load(getFormatSrc('hls'));
                                }

                                scope.vgPlayerReady({$API: api});
                            }, function (error) {
                                $timeout(function () {
                                    scope.errorLoading = true;
                                    scope.loading = false;
                                    scope.flashls = false;// Kill flashls with his error
                                    scope.native = false;
                                    scoep.jsHls = false;
                                });
                                console.error(error);
                            }, function (position, duration) {
                                if (position != 0) {
                                    scope.loading = false;
                                    scope.vgUpdateTime({$currentTime: position, $duration: duration});
                                }
                            });
                        });
                    }
                }

                function initJsHls(){
                    scope.flashls = false;
                    scope.native = false;
                    scope.jsHls = true;
                    console.log("Init js hls");
                    hlsAPI.init( element.find(".videoplayer"), function (api) {
                            scope.vgApi = api;
                            if (scope.vgSrc) {
                                $timeout(function(){
                                    scope.loading = false;
                                });
                                scope.vgApi.load(getFormatSrc('hls'));
                            }
                            scope.vgPlayerReady({$API:api});
                        },  function (api) {
                                console.log(api);
                                console.error("some error");
                    });
                }

                element.bind('contextmenu',function() { return !!scope.debugMode; }); // Kill context menu
                
                
                var format = null;
                function srcChanged(){
                    scope.loading = false;
                    scope.errorLoading = false;
                    if(scope.vgSrc ) {
                        format = detectBestFormat();
                        if(newPlayer(format)){ // Remove or recycle old player.
                            // Some problem happened. We must reload video here
                            console.log("Recycling");
                            $timeout(srcChanged);
                            console.log("Continue");

                            if(!format){
                                scope.native = false;
                                scope.flashls = false;
                                scope.jsHls = false;
                                return;
                            }
                            scope.player = format;
                            switch(format){
                                case "flashls":
                                    initFlashls();
                                    break;

                                case "jshls":
                                    initJsHls();
                                    break;

                                case "rtsp":
                                    initRtsp();
                                    break;

                                case "native-hls":
                                    initNativePlayer("hls");
                                    break;

                                case "webm":
                                default:
                                    initNativePlayer(format);
                                    break;
                            }
                        }
                        else{

                        }
                    }
                }

                scope.$watch("vgSrc",srcChanged);

                scope.$on('$destroy',recyclePlayer);
            }
        }
    }]);
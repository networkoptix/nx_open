'use strict';

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
                vgSrc:"="
            },
            templateUrl: 'views/components/videowindow.html',// ???

            link: function (scope, element/*, attrs*/) {

                var mimeTypes = {
                    'hls': 'application/x-mpegURL',
                    'webm': 'video/webm',
                    'rtsp': 'application/x-rtsp',
                    'flv': 'video/x-flv',
                    'mp4': 'video/mp4'
                };

                scope.debugMode = false;

                function getFormatSrc(mediaformat) {
                    var src = _.find(scope.vgSrc,function(src){return src.type == mimeTypes[mediaformat];});
                    return src?src.src:null;
                }

                function detectBestFormat(){
                    scope.flashRequired = false;
                    scope.flashOrWebmRequired = false;
                    scope.noArmSupport = false;
                    scope.noFormat = false;

                    //return "flashls";

                    //This function gets available sources for camera and chooses the best player for this browser

                    //return "rtsp"; // To debug some format - force it to work

                    //We have options:
                    // webm - for good desktop browsers
                    // webm-codec - for IE. Detect
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
                    var weHaveRtsp = _.find(scope.vgSrc,function(src){return src.type == mimeTypes['rtsp'];});

                    // Test native support. Native is always better choice
                    if(weHaveWebm && canPlayNatively("webm")){ // webm is our best format for now
                        return "webm";
                    }

                    if(weHaveHls && canPlayNatively("hls")){ // webm is our best format for now
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

                    if(window.jscd.mobile && weHaveHls){
                        return "native-hls"; // Only one choice on mobile.
                        // TODO: Try removing this line.
                    }

                    // No native support
                    //Presume we are on desktop:
                    switch(window.jscd.browser){
                        case 'Microsoft Internet Explorer':
                            // Check version here
                            if(weHaveWebm)
                            {
                                scope.ieNoWebm = true;
                            }

                            if(weHaveHls && window.jscd.flashVersion ){ // We have flash - try to play using flash
                                return "flashls";
                            }

                            /*if(window.jscd.browserMajorVersion>=10 && weHaveHls){
                                return "jshls";
                            }*/

                            if(weHaveHls && weHaveWebm){

                                scope.flashOrWebmRequired = true;
                                return false;
                            }
                            if(weHaveHls) {
                                scope.flashRequired = true;
                                return false;
                            }

                            if(weHaveWebm)
                            {
                                scope.edgeNoWebm = true;
                                return false;
                            }

                            scope.noFormat = true;
                            return false; // IE9 - No other supported formats


                        case "Safari": // TODO: Try removing this line.
                            if(weHaveHls) {
                                return "native-hls";
                            }

                        case "Chrome":
                        case "Firefox":
                        case "Opera":
                        case "Webkit":
                        default:
                            if(weHaveHls && window.jscd.flashVersion ){ // We have flash - try to play using flash
                                return "flashls";
                            }
                            /*if(weHaveHls) {
                                return "jshls";// We are hoping that we have some good browser
                            }*/
                            if(weHaveRtsp && window.jscd.flashVersion){
                                return "rtsp";
                            }
                            if(weHaveHls) {
                                scope.flashRequired = true;
                                return false;
                            }

                            scope.noFormat = true;
                            return false; // IE9 - No supported formats
                    }
                }


                //TODO: remove ID, generate it dynamically

                var activePlayer = null;
                function recyclePlayer(player){
                    if(activePlayer != player) {
                        element.find(".videoplayer").html("");
                        scope.vgPlayerReady({$API: null});
                    }
                    activePlayer = player;

                }

                // TODO: Create common interface for each player, html5 compatible or something
                // TODO: move supported info to config
                // TODO: Support new players

                function initNativePlayer(format){

                    scope.native = true;
                    scope.flashls = false;
                    nativePlayer.init(element.find(".videoplayer"), function (api) {
                        scope.vgApi = api;

                        if (scope.vgSrc) {
                            scope.vgApi.load(getFormatSrc(format),mimeTypes[format]);

                            scope.vgApi.addEventListener("timeupdate",function(event,arg2,arg3){
                                var video = event.srcElement || event.originalTarget;
                                scope.vgUpdateTime({$currentTime:video.currentTime, $duration: video.duration});
                                if(scope.loading) {
                                    $timeout(function(){
                                        scope.loading = false;
                                    });
                                }
                            });
                        }

                        scope.vgPlayerReady({$API:scope.vgApi});
                    }, function (api) {
                        console.error("some error");
                    });
                }

                function initFlashls() {
                    scope.flashls = true;
                    scope.native = false;
                    scope.flashSource = "components/flashlsChromeless.swf";
                    scope.flashParam = flashlsAPI.flashParams();

                    if(!flashlsAPI.ready()) {
                        flashlsAPI.init("videowindow", function (api) {
                            scope.vgApi = api;

                            if (scope.vgSrc) {
                                scope.vgApi.load(getFormatSrc('hls'));
                            }

                            scope.vgPlayerReady({$API: api});
                        }, function (error) {
                            scope.errorLoading = true;
                            scope.loading = false;
                            scope.flashls = false;// Kill flashls!
                            scope.native = false;
                            scope.$digest();
                            console.error(error);
                        }, function (position, duration) {
                            scope.loading = false;
                            scope.vgUpdateTime({$currentTime: position, $duration: duration});
                        });
                    }else{
                        flashlsAPI.load(getFormatSrc('hls'));
                    }
                }

                function initJsHls(){
                    jshlsAPI.init( element.find(".videoplayer"), function (api) {
                        scope.vgApi = api;

                        if (scope.vgSrc) {
                            scope.vgApi.load(getFormatSrc('hls'));
                        }

                        scope.vgPlayerReady({$API:api});
                    }, function (api) {
                        console.error("some error");
                    });

                }

                function initRtsp(){
                    var locomote = new Locomote('videowindow', /*'bower_components/locomote/dist/Player.swf'/**/'components/Player.swf'/**/);
                    locomote.on('apiReady', function() {
                        scope.vgApi = locomote;

                        /* Tell Locomote to play the specified media */
                        if(!scope.vgApi.load ) {
                            scope.vgApi.load = scope.vgApi.play;
                            scope.vgApi.play = function(){
                                scope.vgApi.load(getFormatSrc('rtsp'));
                            }
                        }

                        /* Start listening for streamStarted event */
                        locomote.on('streamStarted', function() {
                            //console.log('stream has started');
                        });

                        /* If any error occurs, we should take action */
                        locomote.on('error', function(err) {
                            console.error(err);
                        });

                        if (scope.vgSrc) {
                            //console.log('play',scope.vgSrc[2].src);
                            scope.vgApi.play(scope.vgSrc[2].src);
                        }

                        scope.vgPlayerReady({$API:api});
                    });
                }

                element.bind('contextmenu',function() { return !!scope.debugMode; }); // Kill context menu


                scope.$watch("vgSrc",function(){

                    scope.errorLoading = false;
                    if(/*!scope.vgApi && */scope.vgSrc ) {
                        var format = detectBestFormat();

                        scope.loading = !!format;

                        recyclePlayer(format);// Remove old player. TODO: recycle it later

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
                    //if(scope.vgApi && scope.vgSrc ) {
                    //    scope.vgApi.load(scope.vgSrc[0].src);
                    //}
                });
            }
        }
    }]);
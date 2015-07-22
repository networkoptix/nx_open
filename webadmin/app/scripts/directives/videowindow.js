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
                vgPlayerReady:"&", // $scope.vgPlayerReady({$API: $scope.API});
                vgUpdateTime:"&",
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

                function getFormatSrc(mediaformat) {
                    var src = _.find(scope.vgSrc,function(src){return src.type == mimeTypes[mediaformat];});
                    return src?src.src:null;
                }

                function detectBestFormat(){
                    //This function gets available sources for camera and chooses the best player for this browser

                    //return "rtsp"; // To debug some format - force it to work

                    //We have options:
                    // webm - for good desktop browsers
                    // webm-codec - for IE. Detect
                    // nativehls - for mobile browsers
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

                    if(weHaveWebm && canPlayNatively("webm")){ // webm is our best format for now
                        return "webm";
                    }

                    if(window.jscd.mobile && weHaveHls){
                        return "native-hls"; // Only one choice on mobile
                    }

                    //Presume we are on desktop:
                    switch(window.jscd.browser){
                        case 'Microsoft Internet Explorer':
                            // Check version here
                            if(weHaveWebm )
                            {
                                scope.ieNoWebm = true;
                            }

                            if(window.jscd.browserMajorVersion>=10 && weHaveHls){
                                return "jshls";
                            }

                            return false; // IE9 - No other supported formats

                        case "Chrome":
                        case "Firefox":
                        case "Safari":
                        case "Opera":
                        case "Webkit":
                        default:
                            if(window.jscd.flashVersion != '-'){ // We have flash - try to play using flash
                                return "flashls";
                            }
                            if(weHaveHls) {
                                return "jshls";// We are hoping that we have some good browser
                            }
                            if(weHaveRtsp){
                                return "rtsp";
                            }
                            return false; // IE9 - No supported formats
                    }

                    return false; // No supported formats
                }



                function initVideogular(){
                    scope.videogular = true;
                }

                function initFlashls() {
                    scope.videogular = false;
                    flashlsAPI.init("videowindow", function (api) {
                        console.log("videowindow ready");
                        scope.api = api;
                        scope.vgPlayerReady({$API: scope.api});
                        if (scope.vgSrc) {
                            scope.api.load(getFormatSrc('hls'));
                        }
                    }, function (api) {
                        console.alert("some error");
                    });
                }

                function initJsHls(){
                    scope.videogular = false;

                    jshlsAPI.init( $("#videowindow"), function (api) {
                        console.log("videowindow ready");
                        scope.api = api;
                        scope.vgPlayerReady({$API: scope.api});
                        if (scope.vgSrc) {
                            scope.api.load(getFormatSrc('hls'));
                        }
                    }, function (api) {
                        console.alert("some error");
                    });

                }

                function initRtsp(){
                    scope.videogular = false;
                    var locomote = new Locomote('videowindow', 'bower_components/locomote/dist/Player.swf'/*'components/Player.swf'*/);
                    locomote.on('apiReady', function() {
                        console.log("play rtsp");
                        scope.api = locomote;
                        console.log('API is ready. `play` can now be called');

                        scope.vgPlayerReady({$API: scope.api});
                        /* Tell Locomote to play the specified media */
                        if(!scope.api.load ) {
                            scope.api.load = scope.api.play;
                            scope.api.play = function(){
                                scope.api.load(getFormatSrc('rtsp'));
                            }
                        }
                        /* Start listening for streamStarted event */
                        locomote.on('streamStarted', function() {
                            console.log('stream has started');
                        });

                        /* If any error occurs, we should take action */
                        locomote.on('error', function(err) {
                            console.log(err);
                        });

                        if (scope.vgSrc) {
                            console.log('play',scope.vgSrc[2].src);
                            scope.api.play(scope.vgSrc[2].src);
                        }
                    });
                }



                scope.$watch("vgSrc",function(){
                    console.log("change vgSrc", scope.api, scope.vgSrc);

                    if(!scope.api && scope.vgSrc ) {
                        var format = detectBestFormat();
                        console.log("format",format);
                        switch(detectBestFormat()){
                            case "flashls":
                                initFlashls();
                                break;

                            case "jshls":
                                initJsHls();
                                break;

                            case "rtsp":
                                initRtsp();
                                break;

                            case "webm":
                            case "nativehls":
                            default:
                                initVideogular();
                        }
                    }
                    if(scope.api && scope.vgSrc ) {
                        scope.api.load(scope.vgSrc[0].src);
                    }
                });
            }
        }
    }]);
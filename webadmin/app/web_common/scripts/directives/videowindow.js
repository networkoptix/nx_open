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
angular.module('nxCommon')
    .directive('videowindow', ['$interval','$timeout','animateScope', '$sce', function ($interval,$timeout,animateScope, $sce) {
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
                playing: "=",
                preview: "="
            },
            templateUrl: Config.viewsDirCommon + 'components/videowindow.html',// ???

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
                scope.jshlsHideError = Config.debug.jshlsHideError && Config.allowDebugMode;
                scope.jshlsDebugMode = Config.debug.jshlsDebug && Config.allowDebugMode;
                scope.videoFlags = {};
                scope.loading = false;
                
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
                    scope.loading = false;

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
                            }
                            break;

                        case "Firefox":
                            if(weHaveWebm && canPlayNatively("webm"))
                            {
                                return "webm";
                            }
                            if(weHaveHls && window.jscd.os === 'Linux'){
                                scope.videoFlags.ubuntuNX = true;
                                return false;
                            }

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
                    }

                    scope.videoFlags.flashRequired = true;
                    scope.videoFlags.noFormat = true;
                    return false; // IE9 - No supported formats
                }


                //TODO: remove ID, generate it dynamically

                function recyclePlayer(player){
                    if(scope.player != player || !player) {
                        scope.vgPlayerReady({$API: null});
                        scope.player = player;
                        return false;
                    }
                    return true;
                }

                // TODO: Create common interface for each player, html5 compatible or something
                // TODO: move supported info to config
                // TODO: Support new players

                var videoPlayers = [];

                function initNativePlayer(nativeFormat) {

                    scope.native = true;
                    scope.flashls = false;
                    scope.jsHls = false;

                    var autoshow = null;
                    $timeout(function(){
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
                                    scope.loading = false;
                                    scope.vgUpdateTime({$currentTime: video.currentTime, $duration: video.duration});
                                });

                                scope.vgApi.addEventListener("pause", function(event){
                                    scope.playing = false;
                                });
                                scope.vgApi.addEventListener("play", function(event){
                                    scope.playing = true;
                                });

                                scope.vgApi.addEventListener("ended",function(event){
                                    scope.vgUpdateTime({$currentTime: null, $duration: null});

                                });
                            }

                            scope.vgPlayerReady({$API: scope.vgApi});
                        }, function (api) {
                            console.error("some error");
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
                    
                    scope.flashSource = "web_common/components/flashlsChromeless.swf";
                    if(scope.debugMode && scope.debugFormat){
                        scope.flashSource = "web_common/components/flashlsChromeless_debug.swf";
                    }

                    var flashlsAPI = new FlashlsAPI(null);

                    if(flashlsAPI.ready()){
                        flashlsAPI.kill();
                        scope.flashls = false; // Destroy it!
                        $timeout(initFlashls);
                    }else {
                        $timeout(function () {// Force DOM to refresh here
                            flashlsAPI.init(playerId, function (api) {
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
                                    scope.videoFlags.errorLoading = true;
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

                            videoPlayers.push(flashlsAPI);
                        });
                    }
                }

                function initJsHls(){
                    scope.flashls = false;
                    scope.native = false;
                    scope.jsHls = true;

                    $timeout(function(){
                        var jsHlsAPI = new JsHlsAPI();
                        jsHlsAPI.init( element.find(".videoplayer"), scope.jshlsHideError, scope.jshlsDebugMode, function (api) {
                            scope.vgApi = api;
                            if (scope.vgSrc) {
                                $timeout(function(){
                                scope.loading = !!format;
                                });
                                scope.vgApi.load(getFormatSrc('hls'));
                                scope.vgApi.addEventListener("timeupdate", function (event) {
                                    var video = event.srcElement || event.originalTarget;
                                scope.loading = false;
                                    scope.vgUpdateTime({$currentTime: video.currentTime, $duration: video.duration});
                                });
                            }
                            scope.vgPlayerReady({$API:api});
                        },  function (api) {
                            scope.videoFlags.errorLoading = true;
                                scope.jsHls = false;
                                console.log(api);
                        });
                        videoPlayers.push(jsHlsAPI);
                    });

                }

                element.bind('contextmenu',function() { return !!scope.debugMode; }); // Kill context menu
                

                var format = null;

                function initNewPlayer(){
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
                            initNativePlayer(format);
                            break;
                    }
                }

                function srcChanged(){
                    scope.loading = false;
                    scope.videoFlags.errorLoading = false;

                    if(scope.vgSrc ) {
                        format = detectBestFormat();
                        if(!format){
                            scope.native = false;
                            scope.flashls = false;
                            scope.jsHls = false;
                            return;
                        }
                        if(!recyclePlayer(format)){ // Remove or recycle old player.
                            // Some problem happened. We must reload video here
                            if(scope.vgApi){
                                scope.vgApi.kill();
                            }
                            if(videoPlayers){
                                videoPlayers.pop();
                            }
                            $timeout(initNewPlayer);
                        }
                        else{
                            scope.vgApi.load(getFormatSrc(format == 'webm' ? 'webm' : 'hls'));

                            if(scope.playing){
                                scope.vgApi.play();
                            }

                        }

                        if(scope.rotation != 0 && scope.rotation != 180){
                            updateWidth();
                        }
                    }
                }

                scope.$watch("vgSrc",srcChanged, true);

                scope.$on('$destroy',function(){
                    recyclePlayer();
                    scope.vgApi.kill();
                    if(videoPlayers == 1){
                        videoPlayers.pop();
                    }
                    else{
                        console.error('Problem with deallocating video players');
                        console.error(videoPlayers);
                    }
                });

                if(scope.debugMode)
                    scope.$watch('activeFormat', srcChanged);
                
                scope.initFlash = function(){
                    var playerId = !scope.playerId ? 'player0': scope.playerId;
                    // TODO: Nick, remove html from js code
                    var tmp = '<object classid="clsid:d27cdb6e-ae6d-11cf-96b8-444553540000"codebase="" id="flashvideoembed_'+playerId+'"';
                    tmp += 'width="100%" height="100%">';
                    tmp += '\n\t<param name="movie"  value="'+scope.flashSource+'?inline=1" />';
                    tmp += '\n\t<param name="quality" value="high" />';
                    tmp += '\n\t<param name="swliveconnect" value="true" />';
                    tmp += '\n\t<param name="allowScriptAccess" value="always" />';
                    tmp += '\n\t<param name="bgcolor" value="#1C2327" />';
                    tmp += '\n\t<param name="allowFullScreen" value="true" />';
                    tmp += '\n\t<param name="wmode" value="transparent" />';
                    tmp += '\n\t<param name="FlashVars" value="callback=' + playerId + '" />';
                    tmp += '\n\t<embed src="'+scope.flashSource+'?inline=1" width="100%" height="100%"';
                    tmp += ' name="flashvideoembed_'+playerId+'" quality="high" bgcolor="#1C2327" align="middle" allowFullScreen="true"';
                    tmp += 'allowScriptAccess="always" type="application/x-shockwave-flash" swliveconnect="true" wmode="transparent"';
                    tmp += 'FlashVars="callback='+playerId+'">\n\t</embed>\n</object>';
                    
                    playerId = !scope.playerId ? '' : '#'+playerId;
                    scope.flashPlayer = $sce.trustAsHtml(tmp);
                    // TODO: Nick, that is strange. Why do you do it like this?
                    // TODO: Also, try ng-bind-html instead of setting innerHTML
                };

                scope.getRotation = function(){
                    if(format == "webm"){
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

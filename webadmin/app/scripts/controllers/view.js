'use strict';

angular.module('webadminApp').controller('ViewCtrl',
    function ($scope, $rootScope, $location, $routeParams, mediaserver, cameraRecords, $poll, $q,
              $sessionStorage, $localStorage, currentUser) {
        $scope.currentUser = currentUser;
        if(currentUser === null ){
            return; // Do nothing, user wasn't authorised
        }

        var channels = {
            Auto: 'lo',
            High: 'hi',
            Low: 'lo'
        };
        $scope.debugMode = Config.allowDebugMode;
        $scope.session = $sessionStorage;
        $scope.storage = $localStorage;
        $scope.storage.serverStates = $scope.storage.serverStates || {};
        
        $scope.playerApi = false;
        $scope.storage.cameraId = $routeParams.cameraId || $scope.storage.cameraId   || null;

        if(!$routeParams.cameraId &&  $scope.storage.cameraId){
            $location.path('/view/' + $scope.storage.cameraId, false);
        }

        $scope.treeRequest = null;
        $scope.positionProvider = null;
        $scope.activeVideoRecords = null;
        $scope.liveOnly = true;
        $scope.activeCamera = null;
        $scope.searchCams = '';


        mediaserver.systemSettings().then(function(r){
            $scope.webclientDisabled = r.data.reply.settings.crossdomainEnabled == 'false' ;
        });


        $scope.timeCorrection = 0;
        var minTimeLag = 2000;// Two seconds

        $scope.activeResolution = 'Auto';
        // TODO: detect better resolution here?
        var transcodingResolutions = ['Auto', '1080p', '720p', '640p', '320p', '240p'];
        var nativeResolutions = ['Auto', 'High', 'Low'];
        var onlyHiResolution =  ['Auto', 'High'];
        var onlyLoResolution =  ['Auto', 'Low'];

        var mimeTypes = {
            'hls': 'application/x-mpegURL',
            'webm': 'video/webm',
            'flv': 'video/x-flv',
            'mp4': 'video/mp4',
            'mjpeg':'video/x-motion-jpeg'
        };

        $scope.activeFormat = 'Auto';
        $scope.manualFormats = ['Auto', 'jshls','native-hls','flashls','webm'];
        $scope.availableFormats = [
            'Auto',
            'video/webm',
            'application/x-mpegURL'
        ];

        $scope.settings = {id: ''};
        $scope.volumeLevel = typeof($scope.storage.volumeLevel) === 'number' ? $scope.storage.volumeLevel : 50;

        $scope.serverTime = {};

        mediaserver.getModuleInformation().then(function (r) {
            $scope.settings = {
                id: r.data.reply.id
            };
        });

        if(window.jscd.browser === 'Microsoft Internet Explorer' && ! browserSupports('webm', false, false)){
            if(window.jscd.osVersion < 10) { //For 10th version webm codec doesn't work
                $scope.ieNoWebm = true;
            }
        }

        if(window.jscd.mobile) {
            $scope.mobileStore = window.jscd.os === 'iOS'?'appstore':'googleplay';
            var found = _.find(Config.helpLinks, function (links) {
                if (!links.urls) {
                    return false;
                }
                var url = _.find(links.urls, function (url) {
                    return url.class === $scope.mobileStore;
                });
                if (!url) {
                    return false;
                }
                $scope.mobileAppLink = url.url;
                return true;
            });
            $scope.hasMobileApp = !!found;

        }


        var supportsHls = browserSupports('hls',true,true),
            supportsWebm = browserSupports('webm', true, true);
        function browserSupports(type, maybe, native){
            var v = document.createElement('video');
            if(v.canPlayType && v.canPlayType(mimeTypes[type]).replace(/no/, '')) {
                return true;//Native support
            }
            if(maybe){
                if(type === 'hls' && window.jscd.os !== 'Android' ){
                    return true; // Requires flash, but may be played
                }
                if(type === 'webm' && window.jscd.browser === 'Microsoft Internet Explorer' ){
                    return true; // Requires plugin, but may be played
                }
            }
            if(type === 'hls' && !native){
                return !!window.jscd.flashVersion ; // flash hls support
            }
            return false;
        }
        function cameraSupports(type){
            if(!$scope.activeCamera){
                return null;
            }
            return _.find($scope.activeCamera.mediaStreams,function(stream){
                return stream.transports.indexOf(type)>0;
            });
        }

        function formatSupported(type, nativeOnly){
            return cameraSupports(type) && browserSupports(type, true, nativeOnly);
        }

        function largeResolution (resolution){
            var dimensions = resolution.split('x');
            return dimensions[0] > 1920 || dimensions[1] > 1080;
        }
        function checkiOSResolution(camera){
            var streams = _.find(camera.mediaStreams,function(stream){
                return stream.transports.indexOf('hls')>0 && largeResolution(stream.resolution);
            });
            // Here we have two hls streams
            return !!streams;
        }
        function updateAvailableResolutions() {
            if($scope.player == null){
                $scope.player = supportsHls ? 'hls' : supportsWebm ? 'webm' : null;
            }
            if(!$scope.activeCamera){
                $scope.activeResolution = 'Auto';
                $scope.availableResolutions = ['Auto'];
            }
            //1. Does browser and server support webm?
            if($scope.player != 'webm'){
                $scope.iOSVideoTooLarge = false;

                //1. collect resolutions with hls
                var streams = ['Auto'];
                if($scope.activeCamera) {
                    var availableFormats = _.filter($scope.activeCamera.mediaStreams, function (stream) {
                        return stream.transports.indexOf('hls') > 0;
                    });


                    for (var i = 0; i < availableFormats.length; i++) {
                        if (availableFormats[i].encoderIndex == 0) {
                            if (!( window.jscd.os === 'iOS' && checkiOSResolution($scope.activeCamera) )) {
                                streams.push('High');
                            }
                        }
                        if (availableFormats[i].encoderIndex == 1) {
                            streams.push('Low');
                        }
                    }
                }
                $scope.availableResolutions = streams;

                if($scope.activeCamera && streams.length === 1 ){
                    if(window.jscd.os === 'iOS' ){
                        $scope.iOSVideoTooLarge = true;
                    }else {
                        console.error("no suitable streams from this camera");
                    }
                }
            }
            else
            {
                $scope.availableResolutions = transcodingResolutions;
            }

            if($scope.availableResolutions.indexOf($scope.activeResolution)<0){
                $scope.activeResolution = $scope.availableResolutions[0];
            }
        }


        $scope.playerReady = function(API){
            $scope.playerAPI = API;
            if(API) {
                $scope.switchPlaying(true);
                $scope.playerAPI.volume($scope.volumeLevel);
            }
        };
        $scope.updateVideoSource = function(playing) {
            updateAvailableResolutions();
            var live = !playing;

            $scope.positionSelected = !!playing;
            if(!$scope.positionProvider){
                return;
            }

            if($scope.treeRequest){
                $scope.treeRequest.abort('$scope.updateVideoSource'); //abort tree reloading request to speed up loading new video
            }


            $scope.positionProvider.init(playing, $scope.timeCorrection);
            if(live){
                playing = (new Date()).getTime();
            }else{
                playing = Math.round(playing);
            }
            var cameraId = $scope.activeCamera.physicalId;
            var serverUrl = '';

            var mediaDemo = mediaserver.mediaDemo();
            if(mediaDemo){
                serverUrl = mediaDemo;
            }
            var authParam = '&auth=' + mediaserver.authForMedia();

            var positionMedia = !live ? '&pos=' + (playing) : '';

            var resolution = $scope.activeResolution;
            var resolutionHls = channels[resolution] || channels.Low;

            // Fix here!
            if(resolutionHls === channels.Low && $scope.availableResolutions.indexOf('Low')<0){
                resolutionHls = channels.High;
            }
            $scope.resolution = resolutionHls;

            $scope.currentResolution = $scope.player == "webm" ? resolution : resolutionHls;
            $scope.activeVideoSource = _.filter([
                { src: ( serverUrl + '/hls/'   + cameraId + '.m3u8?'            + resolutionHls + positionMedia + authParam ), type: mimeTypes.hls, transport:'hls'},
                { src: ( serverUrl + '/media/' + cameraId + '.webm?rt&resolution=' + resolution + positionMedia + authParam ), type: mimeTypes.webm, transport:'webm' }
            ],function(src){
                return formatSupported(src.transport,false) && $scope.activeFormat === 'Auto' || $scope.debugMode && $scope.manualFormats.indexOf($scope.activeFormat) > -1;
            });
        };


        $scope.updateTime = function(currentTime, duration){
            if(currentTime === null && duration === null){
                //Video ended
                $scope.switchPosition(false); // go to live here
                return;
            }
            currentTime = currentTime || 0;

            $scope.positionProvider.setPlayingPosition(currentTime*1000);
            /*if(!$scope.positionProvider.liveMode) {
                $location.search('time', Math.round($scope.positionProvider.playedPosition));
            }*/
        };


        $scope.switchPlaying = function(play){
            if($scope.playerAPI) {
                if (play) {
                    $scope.playerAPI.play();
                } else {
                    $scope.playerAPI.pause();
                }
                if ($scope.positionProvider) {
                    $scope.positionProvider.playing = play;

                    if(!play){
                        $timeout(function(){
                            $scope.positionProvider.liveMode = false; // Do it async
                        });
                    }
                }
            }
        };

        $scope.switchPosition = function( val ){

            //var playing = $scope.positionProvider.checkPlayingDate(val);

            //if(playing === false) {
                $scope.updateVideoSource(val);//We have nothing more to do with it.
            /*}else{
                $scope.playerAPI.seekTime(playing); // Jump to buffered video
            }*/
        };

        $scope.selectFormat = function(format){
            $scope.activeFormat = format;
            $scope.updateVideoSource($scope.positionProvider.liveMode?null:$scope.positionProvider.playedPosition);
        };

        $scope.selectResolution = function(resolution){
            /*if(resolution === 'auto' || resolution === 'Auto' || resolution === 'AUTO'){
                resolution = '320p'; //TODO: detect better resolution here
            }*/

            if($scope.activeResolution === resolution){
                return;
            }
            $scope.activeResolution = resolution;
            $scope.updateVideoSource($scope.positionProvider.liveMode?null:$scope.positionProvider.playedPosition);
        };

        $scope.enableFullScreen = screenfull.enabled;
        $scope.fullScreen = function(){
            if (screenfull.enabled) {
                screenfull.request($('.videowindow').get(0));
            }
        };

        document.addEventListener('MSFullscreenChange',function(){ // IE only
            $('.videowindow').toggleClass('fullscreen');
        });


        $scope.$watch('positionProvider.liveMode',function(mode){
            if(mode){
                $scope.positionSelected = false;
            }
        });


        $scope.$watch('activeCamera.status',function(status,oldStatus){

            if(typeof(oldStatus) == "undefined"){
                return;
            }

            if((!$scope.positionProvider || $scope.positionProvider.liveMode) && !(status === 'Offline' || status === 'Unauthorized')){
                $scope.updateVideoSource();
            }
        });

        $scope.$watch('player', function(){
            $scope.updateVideoSource($scope.positionProvider.liveMode?null:$scope.positionProvider.playedPosition)
        },true);

        $scope.$watch('volumeLevel', function(){
            if($scope.playerAPI)
                $scope.playerAPI.volume($scope.volumeLevel);
            $scope.storage.volumeLevel = $scope.volumeLevel;
        });

        mediaserver.getTime().then(function(result){
            var clientDate = new Date();

            var serverTime = parseInt(result.data.reply.utcTime);
            var clientTime = clientDate.getTime();
            if(Math.abs(clientTime - serverTime) > minTimeLag){
                $scope.timeCorrection = clientTime - serverTime;
            }
            
            $scope.serverTime.timeZoneOffset = parseInt(result.data.reply.timeZoneOffset);
            $scope.serverTime.latency = $scope.timeCorrection;

            if(Config.settingsConfig.useServerTime){
                $scope.timeCorrection = $scope.serverTime.timeZoneOffset + clientDate.getTimezoneOffset() * 60000 - $scope.timeCorrection;
            }
        });



        // This hack was meant for IE and iPad to fix some issues with overflow:scroll and height:100%
        // But I kept it for all browsers to avoid future possible bugs in different browsers
        // Now every browser behaves the same way

        var $window = $(window);
        var $top = $('#top');
        var $viewPanel = $('.view-panel');
        var $camerasPanel = $('.cameras-panel');
        var updateHeights = function() {
            var windowHeight = $window.height();
            var topHeight = $top.outerHeight();

            var topAlertHeight = 0;

            var topAlert = $('td.alert');
            if(topAlert.length){
                topAlertHeight = topAlert.outerHeight() + 1; // -1 here is a hack.
            }

            var viewportHeight = (windowHeight - topHeight - topAlertHeight) + 'px';

            $camerasPanel.css('height',viewportHeight );
            $viewPanel.css('height',viewportHeight );

            //One more IE hack.
            if(window.jscd.browser === 'Microsoft Internet Explorer') {
                var videoWidth = $('header').width() - $('.cameras-panel').outerWidth(true) - 1;
                $('videowindow').parent().css('width', videoWidth + 'px');
            }
        };

        updateHeights();
        setTimeout(updateHeights,50);
        $window.resize(updateHeights);

        $scope.mobileAppAlertClose = function(){
            $scope.session.mobileAppNotified  = true;
            setTimeout(updateHeights,50);
        };

        $scope.ieNoWebmAlertClose = function(){
            $scope.session.ieNoWebmNotified = true;
            setTimeout(updateHeights,50);
        };

    });

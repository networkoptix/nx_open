'use strict';

angular.module('nxCommon').controller('ViewCtrl',
            ['$scope', '$rootScope', '$location', '$routeParams', 'cameraRecords',
              'camerasProvider', '$sessionStorage', '$localStorage', '$timeout', 'systemAPI',
    function ($scope, $rootScope, $location, $routeParams, cameraRecords,
              camerasProvider, $sessionStorage, $localStorage, $timeout, systemAPI) {

        var channels = {
            Auto: 'lo',
            High: 'hi',
            Low: 'lo'
        };

        if($scope.system){ // Use system from outer scope (directive)
            systemAPI = $scope.system;
        }
        $scope.systemAPI = systemAPI;

        $scope.debugMode = Config.allowDebugMode;
        $scope.session = $sessionStorage;
        $scope.storage = $localStorage;
        $scope.camerasProvider = camerasProvider.getProvider(systemAPI);
        $scope.storage.serverStates = $scope.storage.serverStates || {};
        
        $scope.playerApi = false;
        $scope.canViewArchive = false;
        $scope.storage.cameraId = $routeParams.cameraId || $scope.storage.cameraId   || null;

        if(!$routeParams.cameraId &&  $scope.storage.cameraId){
            $scope.toggleCameraPanel = false;
            systemAPI.setCameraPath($scope.storage.cameraId);
        }

        $scope.positionProvider = null;
        $scope.activeVideoRecords = null;
        $scope.liveOnly = true;
        $scope.activeCamera = null;

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

        $scope.volumeLevel = typeof($scope.storage.volumeLevel) === 'number' ? $scope.storage.volumeLevel : 50;

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
            if(!$scope.storage.cameraId){
                $scope.toggleCameraPanel = true;
            }

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

            //if there are 4 or more available resolutions then it is webm otherwise its hls
            if($scope.player != "webm"){
                $scope.availableResolutions.sort();
            }

            if($scope.availableResolutions.indexOf($scope.activeResolution)<0){
                $scope.activeResolution = $scope.availableResolutions[0];
            }
        }

        $scope.updateCamera = function (position) {
            var oldTimePosition = null;
            if($scope.positionProvider && !$scope.positionProvider.liveMode){
                oldTimePosition = $scope.positionProvider.playedPosition;
            }

            var camRotation = _.find($scope.activeCamera.addParams,findRotation);
            $scope.rotation = camRotation && camRotation.value ? parseInt(camRotation.value) : 0;

            position = position?parseInt(position):oldTimePosition;

            if ($scope.activeCamera) {
                $scope.positionProvider = cameraRecords.getPositionProvider([$scope.activeCamera.physicalId], systemAPI);
                $scope.activeVideoRecords = cameraRecords.getRecordsProvider([$scope.activeCamera.physicalId], systemAPI, 640);
                $scope.liveOnly = true;
                if($scope.canViewArchive) {
                    $scope.activeVideoRecords.archiveReadyPromise.then(function (hasArchive) {
                        $scope.liveOnly = !hasArchive;
                    });
                }
                updateVideoSource(position);
                $scope.switchPlaying(true);
            }
        };

        $scope.playerReady = function(API){
            $scope.playerAPI = API;
            if(API) {
                setTimeout(function(){
                    $scope.switchPlaying($scope.positionProvider.playing);
                    $scope.playerAPI.volume($scope.volumeLevel);
                },100);
            }
        };

        function updateVideoSource(playing) {
            if($scope.playerAPI) {
                // Pause playing
                $scope.playerAPI.pause();
            }
            updateAvailableResolutions();
            var live = !playing;

            $scope.positionSelected = !!playing;
            if(!$scope.positionProvider){
                return;
            }

            $scope.positionProvider.init(playing, $scope.positionProvider.playing);
            if(live){
                playing = (new Date()).getTime();
            }else{
                playing = Math.round(playing);
            }
            var cameraId = $scope.activeCamera.physicalId;
            var serverUrl = '';

            var resolution = $scope.activeResolution;
            var resolutionHls = channels[resolution] || channels.Low;

            // Fix here!
            if(resolutionHls === channels.Low && $scope.availableResolutions.indexOf('Low')<0){
                resolutionHls = channels.High;
            }
            $scope.resolution = resolutionHls;

            $scope.currentResolution = $scope.player == "webm" ? resolution : resolutionHls;
            $scope.activeVideoSource = _.filter([
                { src: systemAPI.hlsUrl(cameraId, !live && playing, resolutionHls), type: mimeTypes.hls, transport:'hls'},
                { src: systemAPI.webmUrl(cameraId, !live && playing, resolution), type: mimeTypes.webm, transport:'webm' }
            ],function(src){
                return formatSupported(src.transport,false) && $scope.activeFormat === 'Auto' || $scope.debugMode && $scope.manualFormats.indexOf($scope.activeFormat) > -1;
            });
        }


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

        function findRotation(param){
            return param.name === 'rotation';
        }

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
                updateVideoSource(val);//We have nothing more to do with it.
            /*}else{
                $scope.playerAPI.seekTime(playing); // Jump to buffered video
            }*/
        };

        $scope.selectFormat = function(format){
            $scope.activeFormat = format;
            updateVideoSource($scope.positionProvider.liveMode?null:$scope.positionProvider.playedPosition);
        };

        $scope.selectResolution = function(resolution){
            /*if(resolution === 'auto' || resolution === 'Auto' || resolution === 'AUTO'){
                resolution = '320p'; //TODO: detect better resolution here
            }*/

            if($scope.activeResolution === resolution){
                return;
            }
            $scope.activeResolution = resolution;
            updateVideoSource($scope.positionProvider.liveMode?null:$scope.positionProvider.playedPosition);
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
                updateVideoSource();
            }
        });

        //timeFromUrl is used if we have a time from the url if not then set to false
        var timeFromUrl = null;
        $scope.$watch('activeCamera', function(){
            if(!$scope.activeCamera){
                return;
            }
            $scope.storage.cameraId  = $scope.activeCamera.id;
            systemAPI.setCameraPath($scope.activeCamera.id);
            timeFromUrl = timeFromUrl || null;
            $scope.updateCamera(timeFromUrl);
            timeFromUrl = null;
        });

        $scope.$watch('player', function(){
            if(!$scope.player){
                return;
            }
            updateVideoSource($scope.positionProvider.liveMode?null:$scope.positionProvider.playedPosition);
        },true);

        $scope.$watch('volumeLevel', function(){
            if($scope.playerAPI){
                $scope.playerAPI.volume($scope.volumeLevel);
            }
            $scope.storage.volumeLevel = $scope.volumeLevel;
        });


        systemAPI.getTime().then(function(result){
            var serverUtcTime = parseInt(result.data.reply.utcTime);
            var timeZoneOffset = parseInt(result.data.reply.timeZoneOffset);
            serverTime.init(Config.webclient.useServerTime, serverUtcTime, timeZoneOffset);
        });

        function requestResources(){
            $scope.camerasProvider.requestResources().then(function(res){
                if(!$scope.storage.cameraId)
                {
                    $scope.storage.cameraId = $scope.camerasProvider.getFirstCam();

                }
                $scope.activeCamera = $scope.camerasProvider.getCamera($scope.storage.cameraId);

                $scope.ready = true;
                $timeout(updateHeights);
                $scope.camerasProvider.startPoll();
            });
        }

        systemAPI.checkPermissions(Config.globalViewArchivePermission).then(function(result){
            $scope.canViewArchive = result;
            requestResources();
        });

        // This hack was meant for IE and iPad to fix some issues with overflow:scroll and height:100%
        // But I kept it for all browsers to avoid future possible bugs in different browsers
        // Now every browser behaves the same way

        var $window = $(window);
        var $header = $('header');
        var updateHeights = function() {
            var $viewPanel = $('.view-panel');
            var $camerasPanel = $('.cameras-panel');
            var $placeholder = $(".webclient-placeholder .placeholder");
            var windowHeight = $window.height();
            var headerHeight = $header.outerHeight();

            var topAlertHeight = 0;

            var topAlert = $('td.alert');
            //after the user is notified this should not be calculated again
            if(topAlert.length && !$scope.session.mobileAppNotified){
                topAlertHeight = topAlert.outerHeight() + 1; // -1 here is a hack.
            }

            var viewportHeight = (windowHeight - headerHeight - topAlertHeight) + 'px';

            $camerasPanel.css('height',viewportHeight );
            $viewPanel.css('height',viewportHeight );
            $placeholder.css('height',viewportHeight);

            //One more IE hack.
            if(window.jscd.browser === 'Microsoft Internet Explorer') {
                var videoWidth = $header.width() - $camerasPanel.outerWidth(true) - 1;
                $('videowindow').parent().css('width', videoWidth + 'px');
            }
        };

        //wait for the page to load then update
        $timeout(updateHeights);
        $window.resize(updateHeights);
        window.addEventListener("orientationchange",$timeout(updateHeights,200));

        $scope.mobileAppAlertClose = function(){
            $scope.session.mobileAppNotified  = true;
            $timeout(updateHeights,50);
        };

        $('.video-icon.pull-left-5').dropdown();


        var killSubscription = $rootScope.$on('$routeChangeStart', function (event,next) {
            timeFromUrl = $location.search().time;
            $scope.activeCamera = $scope.camerasProvider.getCamera(next.params.cameraId);
        });

        $('html').addClass('webclient-page');
        $scope.$on('$destroy', function( event ) {
            killSubscription();
            $scope.camerasProvider.stopPoll();
            $window.unbind('resize', updateHeights);
            $('html').removeClass('webclient-page');
        });

    }]);

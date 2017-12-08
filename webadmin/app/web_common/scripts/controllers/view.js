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

        $scope.canViewArchive = false;
        $scope.storage.cameraId = $routeParams.cameraId || $scope.storage.cameraId   || null;

        $scope.isWebAdmin = Config.webadminSystemApiCompatibility;
        $scope.cameraLinks = {enabled: $location.search().cameraLinks};

        if(!$routeParams.cameraId && $scope.storage.cameraId){
            systemAPI.setCameraPath($scope.storage.cameraId);
        }

        $scope.positionProvider = null;
        $scope.activeVideoRecords = null;
        $scope.activeCamera = null;

        $scope.showCameraPanel = true;

        $scope.activeResolution = 'Auto';
        // TODO: detect better resolution here?
        var transcodingResolutions = [L.common.resolution.auto, '1080p', '720p', '640p', '320p', '240p'];

        var mimeTypes = {
            'hls': 'application/x-mpegURL',
            'webm': 'video/webm',
            'flv': 'video/x-flv',
            'mp4': 'video/mp4',
            'mjpeg':'video/x-motion-jpeg',
            'jpeg': 'image/jpeg'
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
        }

        function cameraSupports(type){
            if(!$scope.activeCamera){
                return false;
            }
            if(type == 'preview'){
                return true;
            }
            return _.find($scope.activeCamera.mediaStreams,function(stream){
                return stream.transports.indexOf(type)>0;
            });
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
                $scope.availableResolutions = [L.common.resolution.auto];
                return;
            }
            if(!$scope.activeCamera){
                $scope.activeResolution = 'Auto';
                $scope.availableResolutions = [L.common.resolution.auto];
            }
            //1. Does browser and server support webm?
            if($scope.player != 'webm'){
                $scope.iOSVideoTooLarge = false;

                //1. collect resolutions with hls
                var streams = [L.common.resolution.auto];
                if($scope.activeCamera) {
                    var availableFormats = _.filter($scope.activeCamera.mediaStreams, function (stream) {
                        return stream.transports.indexOf('hls') > 0;
                    });


                    for (var i = 0; i < availableFormats.length; i++) {
                        if (availableFormats[i].encoderIndex == 0) {
                            if (!( window.jscd.os === 'iOS' && checkiOSResolution($scope.activeCamera) )) {
                                streams.push(L.common.resolution.high);
                            }
                        }
                        if (availableFormats[i].encoderIndex == 1) {
                            streams.push(L.common.resolution.low);
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
                $scope.availableResolutions.sort();
            }
            else
            {
                $scope.availableResolutions = transcodingResolutions;
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
                $scope.positionProvider = cameraRecords.getPositionProvider([$scope.activeCamera.id], systemAPI);
                $scope.activeVideoRecords = cameraRecords.getRecordsProvider([$scope.activeCamera.id], systemAPI, 640);
                updateVideoSource(position);
                $scope.switchPlaying(true);
            }
        };
        var playerReadyTimeout = null;
        $scope.playerReady = function(API){
            $scope.playerAPI = API;
            if( playerReadyTimeout ){
                $timeout.cancel(playerReadyTimeout);
                playerReadyTimeout = null;
            }
            if(API) {
                playerReadyTimeout = $timeout(function(){
                    $scope.switchPlaying($scope.positionProvider.playing);
                    if($scope.playerAPI){
                        $scope.playerAPI.volume($scope.volumeLevel);
                    }
                }, Config.webclient.playerReadyTimeout);
            }
        };

        function updateVideoSource(playingPosition) {
            if($scope.playerAPI) {
                // Pause playing
                $scope.playerAPI.pause();
                $scope.playerAPI = null;
            }
            updateAvailableResolutions();
            var live = !playingPosition;

            $scope.positionSelected = !!playingPosition;
            if(!$scope.positionProvider){
                return;
            }

            $scope.positionProvider.init(playingPosition, $scope.positionProvider.playing);
            if(live){
                playingPosition = timeManager.nowToDisplay();
            }else{
                playingPosition = Math.round(playingPosition);
            }

            if(!$scope.activeCamera){
                return;
            }
            var salt = '&' + Math.random();
            var cameraId = $scope.activeCamera.id;
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
                { src: systemAPI.hlsUrl(cameraId, !live && playingPosition, resolutionHls) + salt, type: mimeTypes.hls, transport:'hls'},
                { src: systemAPI.webmUrl(cameraId, !live && playingPosition, resolution) + salt, type: mimeTypes.webm, transport:'webm' },
                { src: systemAPI.previewUrl(cameraId, !live && playingPosition, null, window.screen.availHeight) + salt, type: mimeTypes.jpeg, transport:'preview'}
            ],function(src){
                return cameraSupports(src.transport) != null;
            });

            $scope.preview = _.find($scope.activeVideoSource,function(src){return src.type == 'image/jpeg';}).src;
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
                $scope.crashCount = 0;
                updateVideoSource(val);//We have nothing more to do with it.
            /*}else{
                $scope.playerAPI.seekTime(playing); // Jump to buffered video
            }*/
        };

        //On player error update source to cause player to restart
        $scope.crashCount = 0;
        $scope.playerErrorHandler = function(){
            if($scope.crashCount < Config.webclient.maxCrashCount){
                updateVideoSource($scope.positionProvider.liveMode?null:$scope.positionProvider.playedPosition);
                $scope.crashCount += 1;
            }
            else{
                $scope.crashCount = 0;
            }
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
                screenfull.request($('.fullscreen-area').get(0));
            }
        };

        if ($scope.enableFullScreen) {
            screenfull.onchange(function(){
                $scope.isFullscreen = screenfull.isFullscreen;
            });
        }

        switch(window.jscd.browser){
            case "Safari":
            case "Microsoft Internet Explorer":
            case "Microsoft Edge":
                $scope.enableFullscreenNotification = true;
                break;
            default:
                $scope.enableFullscreenNotification = false;
        }

        $scope.closeFullscreen = function(){
            screenfull.exit();
        }

        $scope.showCamerasPanel = function(){
            $scope.showCameraPanel=true;
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
        var timeFromUrl = $routeParams.time || null;
        $scope.$watch('activeCamera', function(){
            if(!$scope.activeCamera){
                return;
            }
            $scope.player = null;
            $scope.crashCount = 0;
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
            $scope.crashCount = 0;
            updateVideoSource($scope.positionProvider.liveMode?null:$scope.positionProvider.playedPosition);
        },true);

        $scope.$watch('volumeLevel', function(){
            if($scope.playerAPI){
                $scope.playerAPI.volume($scope.volumeLevel);
            }
            $scope.storage.volumeLevel = $scope.volumeLevel;
        });

        timeManager.init(Config.webclient.useServerTime);

        //if camera doesnt exist get from camerasProvider
        function setActiveCamera(camera){
            $scope.activeCamera = camera;
            if(!$scope.activeCamera){
                $scope.activeCamera = $scope.camerasProvider.getFirstCam();
            }

            // User server time offset of current server (server camera belongs to)
            if($scope.activeCamera){
                var serverOffset = $scope.camerasProvider.getServerTimeOffset($scope.activeCamera.parentId);
                if(serverOffset){
                    timeManager.setOffset(serverOffset);
                }
            }

            $scope.showCameraPanel = !$scope.activeCamera;
        }

        systemAPI.checkPermissions(Config.globalViewArchivePermission).then(function(result){
            $scope.canViewArchive = result;
            return $scope.camerasProvider.requestResources();
        }).then(function(){
            // instead of requesting gettime once - we request it for all servers to know each timezone
            return $scope.camerasProvider.getServerTimes();
        }).then(function(){
            setActiveCamera($scope.camerasProvider.getCamera($scope.storage.cameraId));

            $scope.ready = true;
            $timeout(updateHeights);
            $scope.camerasProvider.startPoll();
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
        
        $header.click(function() {
            //350ms delay is to give the navbar enough time to collapse
            $timeout(updateHeights,350);
        }); 

        $window.resize(updateHeights);
        window.addEventListener("orientationchange",$timeout(updateHeights,200));

        $scope.mobileAppAlertClose = function(){
            $scope.session.mobileAppNotified  = true;
            $timeout(updateHeights,50);
        };

        var killSubscription = $rootScope.$on('$routeChangeStart', function (event,next) {
            timeFromUrl = $location.search().time;

            setActiveCamera($scope.camerasProvider.getCamera(next.params.cameraId));
        });

        $('html').addClass('webclient-page');
        $scope.$on('$destroy', function( event ) {
            killSubscription();
            $scope.camerasProvider.stopPoll();
            $window.unbind('resize', updateHeights);
            $('html').removeClass('webclient-page');
        });

    }]);

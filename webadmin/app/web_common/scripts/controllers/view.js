(function () {

'use strict';

angular.module('nxCommon').controller('ViewCtrl',
            ['$scope', '$rootScope', '$location', '$routeParams', 'cameraRecords', 'chromeCast', '$q',
              'camerasProvider', '$sessionStorage', '$localStorage', '$timeout', 'systemAPI', 'voiceControl',
    function ($scope, $rootScope, $location, $routeParams, cameraRecords, chromeCast, $q,
              camerasProvider, $sessionStorage, $localStorage, $timeout, systemAPI, voiceControl) {

        var channels = {
            Auto: 'lo',
            High: 'hi',
            Low: 'lo'
        };

        $scope.showSettings = false;
        
        if($scope.system){ // Use system from outer scope (directive)
            systemAPI = $scope.system;
        }
        $scope.systemAPI = systemAPI;

        $scope.betaMode = Config.allowBetaMode;
        $scope.debugMode = Config.allowDebugMode;
        $scope.session = $sessionStorage;
        $scope.storage = $localStorage;
        $scope.camerasProvider = camerasProvider.getProvider(systemAPI);
        $scope.storage.serverStates = $scope.storage.serverStates || {};
        $scope.storage.activeCameras = $scope.storage.activeCameras || {};

        $scope.canViewArchive = false;
        $scope.searchCams = '';
        $scope.storage.cameraId = $routeParams.cameraId || $scope.storage.cameraId   || null;

        $scope.isWebAdmin = Config.webadminSystemApiCompatibility;
        $scope.cameraLinks = {enabled: $location.search().cameraLinks};
        $scope.voiceControls = {enabled: false, showCommands: false};

        if(!$routeParams.cameraId && $scope.storage.cameraId){
            systemAPI.setCameraPath($scope.storage.cameraId);
        }

        $scope.isEmbeded = ($location.path().indexOf('/embed') === 0);
        var nocontrols = !$location.search().nocontrols,
            noheader = !$location.search().noheader,
            nocameras = !$location.search().nocameras;
        
        function setCameraComponentsVisibility () {
            $scope.showTimeline = !$scope.isEmbeded || $scope.isEmbeded && nocontrols;
            $scope.showCameraHeader = !$scope.isEmbeded || $scope.isEmbeded && noheader;
            $scope.showCamerasMenu = !$scope.isEmbeded || $scope.isEmbeded && nocameras;
            $scope.showCameraPanel = $scope.showCamerasMenu;
        }

        setCameraComponentsVisibility();
        
        var castAlert = false;
        $scope.showWarning = function(){
            if(!castAlert){
                alert(L.common.chromeCastWarning);
                castAlert = true;
            }
        };

        $scope.positionProvider = null;
        $scope.activeVideoRecords = null;
        $scope.activeCamera = null;
        $scope.player = null;

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
            if (type === 'preview') {
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
            if($scope.player === null){
                $scope.availableResolutions = [L.common.resolution.auto];
                return;
            }
            if(!$scope.activeCamera){
                $scope.activeResolution = 'Auto';
                $scope.availableResolutions = [L.common.resolution.auto];
            }
            //1. Does browser and server support webm?
            if ($scope.player !== 'webm') {
                $scope.iOSVideoTooLarge = false;

                //1. collect resolutions with hls
                var streams = [L.common.resolution.auto];
                if($scope.activeCamera) {
                    var availableFormats = _.filter($scope.activeCamera.mediaStreams, function (stream) {
                        return stream.transports.indexOf('hls') > 0;
                    });


                    for (var i = 0; i < availableFormats.length; i++) {
                        if (availableFormats[i].encoderIndex === 0) {
                            if (!( window.jscd.os === 'iOS' && checkiOSResolution($scope.activeCamera) )) {
                                streams.push(L.common.resolution.high);
                            }
                        }
                        if (availableFormats[i].encoderIndex === 1) {
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
            else {
                $scope.availableResolutions = transcodingResolutions;
            }

            if($scope.availableResolutions.indexOf($scope.activeResolution)<0){
                $scope.activeResolution = $scope.availableResolutions[0];
            }
        }

        function findRotation(param) {
            return param.name === 'rotation';
        }

        $scope.toggleSettingsMenu = function () {
            $scope.showSettings = !$scope.showSettings;
        };

        function updateVideoSource(playingPosition) {
            // clear preview for next camera
            $scope.preview = '';
    
            if (!$scope.activeCamera) {
                $scope.activeVideoSource = {src: ''};
                return;
            }
            
            var salt = '&' + Math.random(),
                cameraId = $scope.activeCamera.id,
                resolution = $scope.activeResolution,
                resolutionHls = channels[resolution] || channels.Low,
                live = !playingPosition && $scope.activeCamera.status !== 'Unauthorized';
                
            if($scope.playerAPI) {
                // Pause playing
                $scope.playerAPI.pause();
            }
            
            updateAvailableResolutions();

            $scope.positionSelected = !!playingPosition;
            if(!$scope.positionProvider){
                return;
            }

            $scope.positionProvider.init(playingPosition, $scope.positionProvider.playing);
            
            if(live){
                playingPosition = window.timeManager.nowToDisplay();
            } else {
                playingPosition = Math.round(playingPosition);
            }

            // Fix here!
            if(resolutionHls === channels.Low && $scope.availableResolutions.indexOf('Low')<0){
                resolutionHls = channels.High;
            }
            $scope.resolution = resolutionHls;

            $scope.currentResolution = $scope.player === "webm" ? resolution : resolutionHls;
            $scope.activeVideoSource = _.filter([
                {
                    src: systemAPI.hlsUrl(cameraId, !live && playingPosition, resolutionHls) + salt,
                    type: mimeTypes.hls,
                    transport: 'hls'
                },
                {
                    src: systemAPI.webmUrl(cameraId, !live && playingPosition, resolution) + salt,
                    type: mimeTypes.webm,
                    transport: 'webm'
                },
                {
                    src: systemAPI.previewUrl(cameraId, !live && playingPosition, null, window.screen.availHeight) + salt,
                    type: mimeTypes.jpeg,
                    transport: 'preview'
                }
            ],function(src){
                return cameraSupports(src.transport) != null;
            });

            $scope.preview = _.find($scope.activeVideoSource, function (src) {
                return src.type === 'image/jpeg';
            }).src;

            if((Config.allowBetaMode || $scope.debugMode) && window.jscd.browser.toLowerCase() === 'chrome'){
                var streamInfo = {};
                var streamType = 'webm';

                if($scope.debugMode){
                    streamType = $scope.player === 'webm' ? 'webm' : 'hls';
                }

                streamInfo.src = streamType === 'webm' ? systemAPI.webmUrl(cameraId, !live && playingPosition, resolution, true)
                                                         : systemAPI.hlsUrl(cameraId, !live && playingPosition, resolutionHls);
                streamInfo.title = $scope.activeCamera.name;

                if(cameraSupports(streamType) || $scope.debugMode){
                    $scope.showCastButton = true;
                    chromeCast.load(streamInfo, streamType);
                }
                else{
                    $scope.showCastButton = false;
                }
            }
        }

        $scope.updateCamera = function (position) {
            var oldTimePosition = null;
            if ($scope.positionProvider && !$scope.positionProvider.liveMode) {
                oldTimePosition = $scope.positionProvider.playedPosition;
            }
            
            var camRotation = _.find($scope.activeCamera.addParams, findRotation);
            $scope.rotation = camRotation && camRotation.value ? parseInt(camRotation.value) : 0;
            
            position = position ? parseInt(position) : oldTimePosition;
            
            if ($scope.activeCamera) {
                $scope.positionProvider = cameraRecords.getPositionProvider([$scope.activeCamera.id], systemAPI);
                $scope.activeVideoRecords = cameraRecords.getRecordsProvider([$scope.activeCamera.id], systemAPI, 640);
                updateVideoSource(position);
                $scope.switchPlaying(true);
            }
        };
        
        var playerReadyTimeout = null;
        $scope.playerReady = function (API) {
            $scope.playerAPI = API;
            if (playerReadyTimeout) {
                $timeout.cancel(playerReadyTimeout);
                playerReadyTimeout = null;
            }
            if (API) {
                playerReadyTimeout = $timeout(function () {
                    $scope.switchPlaying($scope.positionProvider.playing);
                    if ($scope.playerAPI && !Config.webclient.disableVolume) {
                        $scope.playerAPI.volume($scope.volumeLevel);
                    }
                }, Config.webclient.playerReadyTimeout);
            }
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
                            $scope.positionProvider.liveMode = $scope.positionProvider.isArchiveEmpty(); // Do it async
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

        function handleVideoError(forceLive){
            var showError = $scope.crashCount < Config.webclient.maxCrashCount;
            if (showError) {
                updateVideoSource($scope.positionProvider.liveMode ||
                forceLive ? null : $scope.positionProvider.playedPosition);
                $scope.crashCount += 1;
            }
            else{
                $scope.crashCount = 0;
            }
            return !showError;
        }

        $scope.playerHandler = function(error){
            if(error){
                return $scope.positionProvider.checkEndOfArchive().then(function(jumpToLive){
                    return handleVideoError(jumpToLive);  // Check crash count to reload media
                },function(){
                    return true; // Return true to show error to user
                });
            }
            
            $scope.crashCount = 0;
            return $q.resolve(false);
        };

        $scope.selectFormat = function(format){
            $scope.showSettings = false;
            $scope.activeFormat = format;
            updateVideoSource($scope.positionProvider.liveMode?null:$scope.positionProvider.playedPosition);
        };

        $scope.toggleVoice = function () {
            $scope.showSettings = false;
            $scope.voiceControls.showCommands = !$scope.voiceControls.showCommands;
        };
        
        $scope.selectResolution = function(resolution){
            /*if(resolution === 'auto' || resolution === 'Auto' || resolution === 'AUTO'){
                resolution = '320p'; //TODO: detect better resolution here
            }*/

            $scope.showSettings = false;
            
            if($scope.activeResolution === resolution){
                return;
            }
            $scope.activeResolution = resolution;
            updateVideoSource($scope.positionProvider.liveMode?null:$scope.positionProvider.playedPosition);
        };

        var fullElement = document.getElementById('fullscreen-area');
    
        angular.element(fullElement).on('dblclick', function (event) {
            screenfull.toggle(fullElement);
        });
    
        $scope.enableFullScreen = screenfull.enabled;
        $scope.fullScreen = function(){
            $scope.showSettings = false;
            if (screenfull.enabled) {
                screenfull.request(fullElement);
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
            $scope.showSettings = false;
            screenfull.exit();
        };

        $scope.showCamerasPanel = function(){
            $scope.showSettings = false;
            $scope.showOnTop = true; // z-index -> show panel on small screen

            setCameraComponentsVisibility();
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

            if(typeof(oldStatus) === 'undefined'){
                return;
            }

            if((!$scope.positionProvider || $scope.positionProvider.liveMode) && !(status === 'Offline' || status === 'Unauthorized')){
                updateVideoSource();
            }
        });

        //timeFromUrl is used if we have a time from the url if not then set to false
        var timeFromUrl = $routeParams.time || null;
        
        function resetSystemActiveCamera() {
            if ($scope.isEmbeded) {
                // don't reset storage
                return;
            }
            // remove active camera record for this system (multiple media servers)
            for (var serverId in $scope.camerasProvider.cameras) {
                if ($scope.camerasProvider.cameras.hasOwnProperty(serverId)) {
                    if ($scope.storage.activeCameras[serverId]) {
                        delete $scope.storage.activeCameras[serverId];
                    }
                }
            }
            
            // record actice camera again as only one camera should be selected per system
            $scope.storage.activeCameras[$scope.activeCamera.server.id] = $scope.activeCamera.id;
        }
    
        $scope.$watch('camerasProvider.cameras', function () {
            if (!$scope.activeCamera && Object.keys($scope.camerasProvider.cameras).length !== 0) {
                $scope.activeCamera = $scope.camerasProvider.getFirstAvailableCamera();
            }
        }, true);
        
        $scope.$watch('activeCamera', function(){
            if(!$scope.activeCamera){
                $scope.showCameraPanel = false;
                return;
            }
            
            resetSystemActiveCamera();
            setCameraComponentsVisibility();
            
            $scope.showOnTop = false; // z-index -> hide panel on small screen
            
            $scope.player = null;
            $scope.crashCount = 0;
            
            if (!$scope.isEmbeded) {
                $scope.storage.cameraId  = $scope.activeCamera.id;
                $scope.storage.serverStates[$scope.activeCamera.server.id] = true; // media server status - expanded
            }
            
            systemAPI.setCameraPath($scope.activeCamera.id);
            timeFromUrl = timeFromUrl || null;
            $scope.updateCamera(timeFromUrl);
            timeFromUrl = null;

            //When camera is changed request offset for camera
            var serverOffset = $scope.camerasProvider.getServerTimeOffset($scope.activeCamera.parentId);
            if(serverOffset){
                window.timeManager.setOffset(serverOffset);
            }
        });

        window.timeManager.init(Config.webclient.useServerTime, Config.webclient.useSystemTime);

        function isActive(val) {
            var currentPath = $location.path();
            return currentPath.indexOf(val) >= 0;
        }

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
            var headerHeight = $header.outerHeight() || 0;

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
    
    
        systemAPI.checkPermissions(Config.globalViewArchivePermission)
            .then(function (result) {
                $scope.canViewArchive = result;
                // instead of requesting gettime once - we request it for all servers to know each timezone
                return $scope.camerasProvider.getServerTimes();
            }).then(function () {
                return $scope.camerasProvider.requestResources();
            }).then(function () {
                if (!isActive('embed') && (!$scope.activeCamera || $scope.activeCamera.id !== $scope.storage.cameraId)) {
                    $scope.activeCamera = $scope.camerasProvider.getCamera($scope.storage.cameraId);
                }
                $scope.ready = true;
                $timeout(updateHeights);
                $scope.camerasProvider.startPoll();
                if (($scope.betaMode || $scope.debugMode) && window.jscd.browser.toLowerCase() === 'chrome') {
                    $scope.voiceControls = {enabled: true, showCommands: true};
                    voiceControl.initControls($scope);
                }
            });
        
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

            $scope.activeCamera = $scope.camerasProvider.getCamera(next.params.cameraId);
        });

        $('html').addClass('webclient-page');
        $scope.$on('$destroy', function( event ) {
            killSubscription();
            $scope.camerasProvider.stopPoll();
            $window.unbind('resize', updateHeights);
            $('html').removeClass('webclient-page');
        });

        $scope.$watch('player', function () {
            if (!$scope.player) {
                return;
            }
            $scope.crashCount = 0;
            updateVideoSource($scope.positionProvider.liveMode ? null : $scope.positionProvider.playedPosition);
        }, true);
        
        $scope.$watch('volumeLevel', function () {
            if ($scope.playerAPI) {
                $scope.playerAPI.volume($scope.volumeLevel);
            }
            $scope.storage.volumeLevel = $scope.volumeLevel;
        });
        
    }]);
})();

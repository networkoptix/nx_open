'use strict';

angular.module('webadminApp').controller('ViewCtrl',
    function ($scope,$rootScope,$location,$routeParams,mediaserver,cameraRecords,$timeout,$q,$sessionStorage,$localStorage) {

        var channels = {
            Auto: 'lo',
            High: 'hi',
            Low: 'lo'
        };
        $scope.session = $sessionStorage;
        $scope.storage = $localStorage;
        $scope.storage.serverStates = $scope.storage.serverStates || {};
        
        $scope.playerApi = false;
        $scope.cameras = {};
        $scope.liveOnly = true;
        $scope.storage.cameraId = $routeParams.cameraId || $scope.storage.cameraId   || null;

        if(!$routeParams.cameraId &&  $scope.storage.cameraId){
            $location.path('/view/' + $scope.storage.cameraId, false);
        }

        $scope.activeCamera = null;
        $scope.searchCams = '';


        mediaserver.systemSettings().then(function(r){
            $scope.webclientDisabled = r.data.reply.settings.crossdomainEnabled == 'false' ;
            console.log($scope.webclientDisabled, r.data.reply.settings.crossdomainEnabled);
        });

        var isAdmin = false;
        var canViewLive = false;
        var canViewArchive = false;
        var availableCameras = null;

        var timeCorrection = 0;
        var minTimeLag = 2000;// Two seconds

        $scope.activeResolution = 'Auto';
        // TODO: detect better resolution here?
        var transcodingResolutions = ['Auto', '1080p', '720p', '640p', '320p', '240p'];
        var nativeResolutions = ['Auto', 'High', 'Low'];
        var onlyHiResolution =  ['Auto', 'High'];
        var onlyLoResolution =  ['Auto', 'Low'];

        var reloadInterval = 5*1000;//30 seconds
        var quickReloadInterval = 3*1000;// 3 seconds if something was wrong
        var mimeTypes = {
            'hls': 'application/x-mpegURL',
            'webm': 'video/webm',
            'rtsp': 'application/x-rtsp',
            'flv': 'video/x-flv',
            'mp4': 'video/mp4',
            'mjpeg':'video/x-motion-jpeg'
        };

        $scope.activeFormat = 'Auto';
        $scope.availableFormats = [
            'Auto',
            'video/webm',
            'application/x-mpegURL',
            'application/x-rtsp'
        ];

        $scope.settings = {id: ''};

        mediaserver.getSettings().then(function (r) {
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
            if(!$scope.activeCamera){
                $scope.activeResolution = 'Auto';
                $scope.availableResolutions = ['Auto'];
            }

            //1. Does browser and server support webm?
            if(!formatSupported('webm',false) || formatSupported('hls',true) && browserSupports('hls', false, true)
                || Config.allowDebugMode && Config.debug.videoFormat == "flashls"){
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
        updateAvailableResolutions();



        function getServer(id) {
            if(!$scope.mediaServers) {
                return null;
            }
            return _.find($scope.mediaServers, function (server) {
                return server.id === id;
            });
        }
        function getCamera(id){
            if(!$scope.cameras) {
                return null;
            }
            for(var serverId in $scope.cameras) {
                var cam = _.find($scope.cameras[serverId], function (camera) {
                    return camera.id === id || camera.physicalId === id;
                });
                if(cam){
                    return cam;
                }
            }
            return null;
        }

        $scope.playerReady = function(API){
            $scope.playerAPI = API;
            if(API) {
                $scope.switchPlaying(true);
            }
        };
        function updateVideoSource(playing) {
            updateAvailableResolutions();
            var live = !playing;

            $scope.positionSelected = !!playing;

            if(!$scope.positionProvider){
                return;
            }

            if(treeRequest){
                treeRequest.abort('updateVideoSource'); //abort tree reloading request to speed up loading new video
            }


            $scope.positionProvider.init(playing);
            if(live){
                playing = (new Date()).getTime();
            }else{
                playing = Math.round(playing);
            }
            var cameraId = $scope.activeCamera.physicalId;
            var serverUrl = '';
            var rtspUrl = 'rtsp://' + window.location.host;

            var mediaDemo = mediaserver.mediaDemo();
            if(mediaDemo){
                serverUrl = mediaDemo;
                rtspUrl = 'rtsp:' + mediaDemo;
            }
            var authParam = '&auth=' + mediaserver.authForMedia();
            var rstpAuthPararm = '&auth=' + mediaserver.authForRtsp();

            var positionMedia = !live ? '&pos=' + (playing) : '';

            var resolution = $scope.activeResolution;
            var resolutionHls = channels[resolution] || channels.Low;


            // Fix here!
            if(resolutionHls === channels.Low && $scope.availableResolutions.indexOf('Low')<0){
                resolutionHls = channels.High;
            }


            $scope.acitveVideoSource = _.filter([
                { src: ( serverUrl + '/hls/'   + cameraId + '.m3u8?'            + resolutionHls + positionMedia + authParam ), type: mimeTypes.hls, transport:'hls'},
                { src: ( serverUrl + '/media/' + cameraId + '.webm?rt&resolution=' + resolution + positionMedia + authParam ), type: mimeTypes.webm, transport:'webm' },

                // Not supported:
                // { src: ( serverUrl + '/media/' + cameraId + '.mpjpeg?resolution=' + $scope.activeResolution + positionMedia + extParam ), type: mimeTypes.mjpeg , transport:'mjpeg'},

                // Require plugin
                { src: ( rtspUrl + '/' + cameraId + '?' + positionMedia + rstpAuthPararm  + '&stream=' + ($scope.activeResolution === 'Low'?1:0)), type: mimeTypes.rtsp, transport:'rtsp'}
            ],function(src){
                return formatSupported(src.transport,false) && $scope.activeFormat === 'Auto'|| $scope.activeFormat === src.type;
            });
        }

        $scope.activeVideoRecords = null;
        $scope.positionProvider = null;

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

        $scope.selectCameraById = function (cameraId, position, silent) {
            if($scope.activeCamera && ($scope.activeCamera.id === cameraId || $scope.activeCamera.physicalId === cameraId)){
                return;
            }
            var oldTimePosition = null;
            if($scope.positionProvider && !$scope.positionProvider.liveMode){
                oldTimePosition = $scope.positionProvider.playedPosition;
            }

            $scope.storage.cameraId  = cameraId || $scope.storage.cameraId  ;

            position = position?parseInt(position):oldTimePosition;

            $scope.activeCamera = getCamera ($scope.storage.cameraId  );
            if (!silent && $scope.activeCamera) {
                $scope.positionProvider = cameraRecords.getPositionProvider([$scope.activeCamera.physicalId], timeCorrection);
                $scope.activeVideoRecords = cameraRecords.getRecordsProvider([$scope.activeCamera.physicalId], 640, timeCorrection);

                $scope.liveOnly = true;
                if(canViewArchive) {
                    $scope.activeVideoRecords.archiveReadyPromise.then(function (hasArchive) {
                        $scope.liveOnly = !hasArchive;
                    });
                }

                updateVideoSource(position);
                updateAvailableResolutions();
                $scope.switchPlaying(true);
            }
        };
        $scope.selectCamera = function (activeCamera) {
            $location.path('/view/' + activeCamera.id, false);
            $scope.selectCameraById(activeCamera.id,false);
        };

        $scope.toggleServerCollapsed = function(server){
            server.collapsed = !server.collapsed;
            $scope.storage.serverStates[server.id] = server.collapsed;
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



        function searchCams(){
            function has(str, substr){
                return str && str.toLowerCase().indexOf(substr.toLowerCase()) >= 0;
            }
            _.forEach($scope.mediaServers,function(server){
                var cameras = $scope.cameras[server.id];
                var camsVisible = false;
                _.forEach(cameras,function(camera){
                    camera.visible = $scope.searchCams === '' ||
                            has(camera.name, $scope.searchCams) ||
                            has(camera.url, $scope.searchCams);
                    camsVisible = camsVisible || camera.visible;
                });

                server.visible = $scope.searchCams === '' ||
                    camsVisible /*||
                    has(server.name, $scope.searchCams) ||
                    has(server.url, $scope.searchCams)*/;
            });
        }


        function extractDomain(url) {
            url = url.split('/')[2] || url.split('/')[0];
            return url.split(':')[0].split('?')[0];
        }
        function objectOrderName(object){
            var num = 0;
            if(object.url) {
                var addrArray = object.url.split('.');
                for (var i = 0; i < addrArray.length; i++) {
                    var power = 3 - i;
                    num += ((parseInt(addrArray[i]) % 256 * Math.pow(256, power)));
                }
                if (isNaN(num)) {
                    num = object.url;
                } else {
                    num = num.toString(16);
                    if (num.length < 8) {
                        num = '0' + num;
                    }
                }
            }

            return object.name + '__' + num;
        }

        var treeRequest = null;

        function getCameras() {

            var deferred = $q.defer();
            if(treeRequest){
                treeRequest.abort('getCameras');
            }
            treeRequest = mediaserver.getCameras();
            treeRequest.then(function (data) {
                var cameras = data.data;
                
                var findMediaStream = function(param){
                    return param.name === 'mediaStreams';
                };
                
                function cameraFilter(camera){
                    // Filter desktop cameras here
                    if(camera.typeId === desktopCameraTypeId){ // Hide desctop cameras
                        return false;
                    }

                    if(isAdmin){
                        return true;
                    }

                    if(availableCameras){
                        return availableCameras.indexOf(camera.id)>=0;
                    }
                    return false;
                }

                function cameraSorter(camera) {
                    camera.url = extractDomain(camera.url);
                    camera.preview = mediaserver.previewUrl(camera.physicalId, false, null, 256);
                    camera.server = getServer(camera.parentId);
                    if(camera.server && camera.server.status === 'Offline'){
                        camera.status = 'Offline';
                    }

                    var mediaStreams = _.find(camera.addParams,findMediaStream);
                    camera.mediaStreams = mediaStreams?JSON.parse(mediaStreams.value).streams:[];

                    if(typeof(camera.visible) === 'undefined'){
                        camera.visible = true;
                    }

                    return objectOrderName(camera);
                }

                function newCamerasFilter(camera){
                    var oldCamera = getCamera(camera.id);
                    if(!oldCamera){
                        return true;
                    }

                    $.extend(oldCamera,camera);
                    return false;
                }
                /*

                 // This is for encoders (group cameras):

                 //1. split cameras with groupId and without
                 var cams = _.partition(cameras, function (cam) {
                 return cam.groupId === '';
                 });

                 //2. sort groupedCameras
                 cams[1] = _.sortBy(cams[1], cameraSorter);

                 //3. group groupedCameras by groups ^_^
                 cams[1] = _.groupBy(cams[1], function (cam) {
                 return cam.groupId;
                 });

                 //4. Translate into array
                 cams[1] = _.values(cams[1]);

                 //5. Emulate cameras by groups
                 cams[1] = _.map(cams[1], function (group) {
                 return {
                 isGroup: true,
                 collapsed: false,
                 parentId: group[0].parentId,
                 name: group[0].groupName,
                 id: group[0].groupId,
                 url: group[0].url,
                 status: 'Online',
                 cameras: group
                 };
                 });

                 //6 union cameras back
                 cameras = _.union(cams[0], cams[1]);
                 */


                cameras = _.filter(cameras, cameraFilter);
                //7 sort again
                cameras = _.sortBy(cameras, cameraSorter);

                //8. Group by servers
                var camerasByServers = _.groupBy(cameras, function (camera) {
                    return camera.parentId;
                });

                if(!$scope.cameras) {
                    $scope.cameras = camerasByServers;
                }else{
                    for(var serverId in $scope.cameras){
                        var activeCameras = $scope.cameras[serverId];
                        var newCameras = camerasByServers[serverId];

                        for(var i = 0; i < activeCameras.length; i++) { // Remove old cameras
                            if(!_.find(newCameras,function(camera){
                                    return camera.id === activeCameras[i].id;
                                }))
                            {
                                activeCameras.splice(i, 1);
                                i--;
                            }
                        }



                        // Merge actual cameras
                        var newCameras = _.filter(newCameras,newCamerasFilter); //Process old cameras and get new

                        // Add new cameras

                        _.forEach(newCameras,function(camera){ // Add new camera
                            activeCameras.push(camera);
                        });
                    }

                    for(var serverId in camerasByServers){
                        if(!$scope.cameras[serverId]){
                            $scope.cameras[serverId] = camerasByServers[serverId];
                        }
                    }
                }

                deferred.resolve(cameras);
            }, function (error) {
                deferred.reject(error);
            });

            return deferred.promise;
        }

        function reloadTree(){
            function serverSorter(server){
                server.url = extractDomain(server.url);
                server.collapsed = $scope.storage.serverStates[server.id];


                if(typeof(server.visible) === 'undefined'){
                    server.visible = true;
                }

                return objectOrderName(server);
            }

            function newServerFilter(server){
                var oldserver = getServer(server.id);

                server.collapsed = oldserver ? oldserver.collapsed :
                    $scope.storage.serverStates[server.id] ||
                    server.status !== 'Online' && (server.allowAutoRedundancy || server.flags.indexOf('SF_Edge') < 0);

                if(oldserver){ // refresh old server
                    $.extend(oldserver,server);
                }

                return !oldserver;
            }

            var deferred = $q.defer();

            if(treeRequest){
                treeRequest.abort('reloadTree');
            }
            treeRequest = mediaserver.getMediaServers();
            treeRequest.then(function (data) {

                if(!$scope.mediaServers) {
                    $scope.mediaServers = _.sortBy(data.data,serverSorter);
                }else{
                    var servers = data.data;
                    for(var i = 0; i < $scope.mediaServers.length; i++) { // Remove old servers
                        if(!_.find(servers,function(server){
                                return server.id === $scope.mediaServers[i].id;
                            }))
                        {
                            $scope.mediaServers.splice(i, 1);
                            i--;
                        }
                    }


                    var newServers = _.filter(servers,newServerFilter); // Process all servers - refresh old and find new

                    _.forEach(_.sortBy(newServers,serverSorter),function(server){ // Add new server
                        $scope.mediaServers.push(server);
                    });
                }

                getCameras().then(function(data){
                        searchCams();
                        deferred.resolve(data);
                    },
                    function(error){
                        searchCams();
                        deferred.reject(error);
                    });

            }, function (error) {
                deferred.reject(error);
            });

            return deferred.promise;
        }

        var firstTime = true;
        var timer = false;
        function reloader(){
            reloadTree().then(function(){
                $scope.selectCameraById($scope.storage.cameraId  , firstTime && $location.search().time || false, !firstTime);
                firstTime = false;
                timer = $timeout(reloader, reloadInterval);
            },function(error){
                if(typeof(error.status) === 'undefined' || !error.status) {
                    console.error(error);
                }
                timer = $timeout(reloader, reloadInterval); // Some error happened. Maybe, request was aborted. Wait and try again
            });
        }
        var desktopCameraTypeId = null;
        function requestResourses() {
            mediaserver.getResourceTypes().then(function (result) {
                desktopCameraTypeId = _.find(result.data, function (type) {
                    return type.name === 'SERVER_DESKTOP_CAMERA';
                });
                desktopCameraTypeId = desktopCameraTypeId ? desktopCameraTypeId.id : null;
                reloader();
            });
        }

        $scope.$watch('positionProvider.liveMode',function(mode){
            if(mode){
                $scope.positionSelected = false;
            }
        });

        $scope.$watch('searchCams',searchCams);

        $scope.$watch('activeCamera.status',function(status,oldStatus){

            if(typeof(oldStatus) == "undefined"){
                return;
            }

            if((!$scope.positionProvider || $scope.positionProvider.liveMode) && !(status === 'Offline' || status === 'Unauthorized')){
                updateVideoSource();
            }
        });

        mediaserver.getTime().then(function(result){
            var serverTime = parseInt(result.data.reply.utcTime);
            var clientTime = (new Date()).getTime();
            if(Math.abs(clientTime - serverTime) > minTimeLag){
                timeCorrection = clientTime - serverTime;
            }
        });

        mediaserver.getCurrentUser().then(function(result){
            isAdmin = result.data.reply.isAdmin || (result.data.reply.permissions & Config.globalEditServersPermissions);
            canViewLive = result.data.reply.isAdmin || (result.data.reply.permissions & Config.globalViewLivePermission);
            canViewArchive = result.data.reply.isAdmin || (result.data.reply.permissions & Config.globalViewArchivePermission);

            if(!canViewLive){
                return;
            }

            var userId = result.data.reply.id;

            if(isAdmin){
                requestResourses(); //Show  whole tree
                return;
            }
            mediaserver.getLayouts().then(function(data){

                availableCameras = _.chain(data.data).
                    filter(function(layout){
                        return layout.parentId === userId;
                    }).
                    map(function(layout){
                        return layout.items;
                    }).
                    flatten().
                    map(function(item){
                        return item.resourceId;
                    }).uniq().value();

                requestResourses();
            });



        });

        $scope.$on(
            '$destroy',
            function( ) {
                $timeout.cancel(timer);
            }
        );


        $rootScope.$on('$routeChangeStart', function (event, next/*, current*/) {
            $scope.selectCameraById(next.params.cameraId, $location.search().time || false);
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

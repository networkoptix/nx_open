'use strict';

angular.module('webadminApp').controller('ViewCtrl',
    function ($scope,$rootScope,$location,$routeParams,mediaserver,cameraRecords,$timeout,$q) {

        $scope.playerApi = false;
        $scope.cameras = {};
        $scope.liveOnly = true;
        $scope.cameraId = $scope.cameraId || $routeParams.cameraId || null;
        $scope.activeCamera = null;

        var isAdmin = false;
        var canViewLive = false;
        var canViewArchive = false;
        var availableCameras = null;

        $scope.activeResolution = 'Auto';
        // TODO: detect better resolution here?
        var transcodingResolutions = ['Auto', '1080p', '720p', '640p', '320p', '240p'];
        var nativeResolutions = ['Auto', 'hi', 'lo'];
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

        $scope.settings = {id: ""};

        mediaserver.getSettings().then(function (r) {
            $scope.settings = {
                id: r.data.reply.id
            };
        });

        if(window.jscd.browser == 'Microsoft Internet Explorer' && ! browserSupports('webm')){
            $scope.ieNoWebm = true;
        }

        if(window.jscd.mobile) {
            $scope.mobileStore = window.jscd.os == "iOS"?"appstore":"googleplay";
            var found = _.find(Config.helpLinks, function (links) {
                if (!links.urls) {
                    return false;
                }
                var url = _.find(links.urls, function (url) {
                    return url.class == $scope.mobileStore;
                });
                if (!url) {
                    return false;
                }
                $scope.mobileAppLink = url.url;
                return true;
            });
            $scope.hasMobileApp = !!found;
        }


        function browserSupports(type){
            var v = document.createElement('video');
            if(v.canPlayType && v.canPlayType(mimeTypes[type]).replace(/no/, '')) {
                return true;//Native support
            }

            if(type=='hls'){
                return window.jscd.flashVersion != '-'; // flash hls support
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

        function formatSupported(type){
            return cameraSupports(type) && browserSupports(type);
        }
        function updateAvailableResolutions() {
            if(!$scope.activeCamera){
                $scope.activeResolution = 'Auto';
                $scope.availableResolutions = ['Auto'];
            }

            //1. Does browser and server support webm?
            if(formatSupported('webm')){
                $scope.availableResolutions = transcodingResolutions;
            }else{
                $scope.availableResolutions = nativeResolutions;
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
                    return camera.id === id;
                });
                if(cam){
                    return cam;
                }
            }
            return null;
        }

        $scope.playerReady = function(API){
            $scope.playerAPI = API;
            $scope.switchPlaying(true);
        };
        function updateVideoSource(playing) {
            var live = !playing;

            $scope.positionSelected = !!playing;

            if(!$scope.positionProvider){
                return;
            }

            $scope.positionProvider.init(playing);
            if(live){
                playing = (new Date()).getTime();
            }
            var cameraId = $scope.activeCamera.physicalId;
            var serverUrl = '';
            var rtspUrl = 'rtsp://' + window.location.host;

            var mediaDemo = mediaserver.mediaDemo();
            if(mediaDemo){
                serverUrl = mediaDemo;
                rtspUrl = "rtsp:" + mediaDemo;
            }
            var authParam = "&auth=" + mediaserver.authForMedia();
            var rstpAuthPararm = "&auth=" + mediaserver.authForRtsp();

            var positionMedia = !live ? "&pos=" + (playing) : "";
            var positionHls = !live ? "&startTimestamp=" + (playing) : "";

            // TODO: check resolution ? 
            $scope.acitveVideoSource = _.filter([
                { src: ( serverUrl + '/hls/'   + cameraId + '.m3u8?'            + $scope.activeResolution + positionHls   + authParam ), type: mimeTypes['hls'], transport:'hls'},
                { src: ( serverUrl + '/media/' + cameraId + '.webm?resolution=' + $scope.activeResolution + positionMedia + authParam ), type: mimeTypes['webm'], transport:'webm' },

                // Not supported:
                // { src: ( serverUrl + '/media/' + cameraId + '.mpjpeg?resolution=' + $scope.activeResolution + positionMedia + extParam ), type: mimeTypes['mjpeg'] , transport:'mjpeg'},

                // Require plugin
                { src: ( rtspUrl + '/' + cameraId + '?' + positionMedia + rstpAuthPararm  + '&stream=' + ($scope.activeResolution=='lo'?1:0)), type: mimeTypes['rtsp'], transport:'rtsp'}
            ],function(src){
                return formatSupported(src.transport) && $scope.activeFormat == 'Auto'|| $scope.activeFormat == src.type;
            });
        }

        $scope.activeVideoRecords = null;
        $scope.positionProvider = null;

        $scope.updateTime = function(currentTime, duration){
            currentTime = currentTime || 0;

            $scope.positionProvider.setPlayingPosition(currentTime*1000);
            /*if(!$scope.positionProvider.liveMode) {
                $location.search('time', Math.round($scope.positionProvider.playedPosition));
            }*/
        };

        $scope.selectCameraById = function (cameraId, position, silent) {


            $scope.cameraId = cameraId || $scope.cameraId;

            if(position){
                position = parseInt(position);
            }

            $scope.activeCamera = getCamera ($scope.cameraId);
            if (!silent && $scope.activeCamera) {
                $scope.positionProvider = cameraRecords.getPositionProvider([$scope.activeCamera.physicalId]);
                $scope.activeVideoRecords = cameraRecords.getRecordsProvider([$scope.activeCamera.physicalId], 640);

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

        $scope.switchPlaying = function(play){
            if($scope.playerAPI) {
                if (play) {
                    $scope.playerAPI.play();
                } else {
                    $scope.playerAPI.pause();
                }
                if ($scope.positionProvider) {
                    $scope.positionProvider.playing = play;
                }
            }
        };

        $scope.switchPosition = function( val ){

            //var playing = $scope.positionProvider.checkPlayingDate(val);

            //console.log("switchPosition", new Date(val), playing);
            //if(playing === false) {
                updateVideoSource(val);//We have nothing more to do with it.
            /*}else{
                console.log("try to seek time",playing/1000);
                $scope.playerAPI.seekTime(playing); // Jump to buffered video
            }*/
        };

        $scope.selectFormat = function(format){
            $scope.activeFormat = format;
            updateVideoSource($scope.positionProvider.liveMode?null:$scope.positionProvider.playedPosition);
        };

        $scope.selectResolution = function(resolution){
            /*if(resolution == "auto" || resolution == "Auto" || resolution == "AUTO"){
                resolution = "320p"; //TODO: detect better resolution here
            }*/

            if($scope.activeResolution == resolution){
                return;
            }
            $scope.activeResolution = resolution;
            updateVideoSource($scope.positionProvider.liveMode?null:$scope.positionProvider.playedPosition);
        };

        $scope.enableFullScreen = screenfull.enabled;
        $scope.fullScreen = function(){
            if (screenfull.enabled) {
                screenfull.request($(".videowindow").get(0));
            }
        };





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
        function getCameras() {

            var deferred = $q.defer();

            mediaserver.getCameras().then(function (data) {
                var cameras = data.data;
                
                var findMediaStream = function(param){
                    return param.name == "mediaStreams";
                };
                
                function cameraFilter(camera){
                    // Filter desktop cameras here
                    if(camera.typeId == desktopCameraTypeId){ // Hide desctop cameras
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
                    if(camera.server.status == 'Offline'){
                        camera.status = 'Offline';
                    }

                    var mediaStreams = _.find(camera.addParams,findMediaStream);
                    camera.mediaStreams = mediaStreams?JSON.parse(mediaStreams.value).streams:[];

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
                                    return camera.id == activeCameras[i].id;
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

                return objectOrderName(server);
            }

            function newServerFilter(server){
                var oldserver = getServer(server.id);

                server.collapsed = oldserver? oldserver.collapsed : server.status !== 'Online' && (server.allowAutoRedundancy || server.flags.indexOf('SF_Edge') < 0);

                if(oldserver){ // refresh old server
                    $.extend(oldserver,server);
                }

                return !oldserver;
            }

            var deferred = $q.defer();

            mediaserver.getMediaServers().then(function (data) {

                if(!$scope.mediaServers) {
                    $scope.mediaServers = _.sortBy(data.data,serverSorter);
                }else{
                    var servers = data.data;
                    for(var i = 0; i < $scope.mediaServers.length; i++) { // Remove old servers
                        if(!_.find(servers,function(server){
                                return server.id == $scope.mediaServers[i].id;
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
                        deferred.resolve(data);
                    },
                    function(error){
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
                $scope.selectCameraById($scope.cameraId, firstTime && $location.search().time || false, !firstTime);
                firstTime = false;
                timer = $timeout(reloader, reloadInterval);
            },function(error){
                console.error(error);
                //timer = $timeout(reloader, quickReloadInterval);
            });
        }
        var desktopCameraTypeId = null;
        function requestResourses() {
            mediaserver.getResourceTypes().then(function (result) {
                desktopCameraTypeId = _.find(result.data, function (type) {
                    return type.name == 'SERVER_DESKTOP_CAMERA';
                });
                desktopCameraTypeId = desktopCameraTypeId ? desktopCameraTypeId.id : null;
                reloader();
            });
        }

        $scope.$watch("positionProvider.liveMode",function(mode){
            if(mode){
                $scope.positionSelected = false;
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
                        return layout.parentId == userId;
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
        var $top = $("#top");
        var $viewPanel = $(".view-panel");
        var $camerasPanel = $(".cameras-panel");
        var updateHeights = function() {
            var windowHeight = $window.height();
            var topHeight = $top.height();

            var topAlertHeight = 0;

            var topAlert = $("td.alert");
            if(topAlert.length){
                topAlertHeight = topAlert.height();
            }

            var viewportHeight = (windowHeight - topHeight - topAlertHeight) + "px";

            $camerasPanel.css("height",viewportHeight );
            $viewPanel.css("height",viewportHeight );
        };

        updateHeights();
        setTimeout(updateHeights,50);
        $window.resize(updateHeights);

        $scope.mobileAppAlertClose = function(){
            $scope.mobileAppNotified  = true;
            setTimeout(updateHeights,50);
        };

        $scope.ieNoWebmAlertClose = function(){
            $scope.ieNoWebmNotified = true;
            setTimeout(updateHeights,50);
        };

    });

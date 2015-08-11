'use strict';

angular.module('webadminApp').controller('ViewCtrl',
    function ($scope,$rootScope,$location,$routeParams,mediaserver,cameraRecords,$timeout,$q) {

        $scope.playerApi = false;
        $scope.cameras = {};
        $scope.activeCamera = null;

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

        if(window.jscd.browser == 'Microsoft Internet Explorer' && ! browserSupports('webm')){
            $scope.ieNoWebm = true;
        }

        $scope.activeResolution = 'Auto';
        // detect max resolution here?
        var transcodingResolutions = ['Auto', '1080p', '720p', '640p', '320p', '240p'];
        var nativeResolutions = ['Auto', 'hi', 'lo'];
        var reloadInterval = 30*1000;//30 seconds
        var quickReloadInterval = 3*1000;// 3 seconds if something was wrong
        var mimeTypes = {
            'hls': 'application/x-mpegURL',
            'webm': 'video/webm',
            'rtsp': 'application/x-rtsp',
            'flv': 'video/x-flv',
            'mp4': 'video/mp4',
            'mjpeg':'video/x-motion-jpeg'
        };

        function cameraSupports(type){
            if(!$scope.activeCamera){
                return null;
            }
            return _.find($scope.activeCamera.mediaStreams,function(stream){
                return stream.transports.indexOf(type)>0;
            });
        }
        function browserSupports(type){

            var res = false;
            var v = document.createElement('video');
            if(v.canPlayType && v.canPlayType(mimeTypes[type]).replace(/no/, '')) {
                return true;//Native support
            }

            if(type=='hls'){
                return window.jscd.flashVersion != '-'; // flash hls support
            }

            return false;
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

        function getServer(id) {
            if(!$scope.mediaServers) {
                return null;
            }
            return _.find($scope.mediaServers, function (server) {
                return server.id === id;
            });
        }

        $scope.playerReady = function(API){
            $scope.playerAPI = API;
            $scope.switchPlaying(true);
        };
        function updateVideoSource(playing) {
            var live = !playing;

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
                { src: ( serverUrl + '/hls/'   + cameraId + '.m3u8?' + positionHls + authParam + '&chunked&' + $scope.activeResolution), type: mimeTypes['hls'], transport:'hls'},
                { src: ( serverUrl + '/media/' + cameraId + '.webm?resolution='   + $scope.activeResolution + positionMedia + authParam ), type: mimeTypes['webm'], transport:'webm' },

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

            $scope.activeCamera = _.find($scope.allcameras, function (camera) {
                return camera.id === $scope.cameraId;
            });

            if (!silent && $scope.activeCamera) {
                $scope.positionProvider = cameraRecords.getPositionProvider([$scope.activeCamera.physicalId]);
                $scope.activeVideoRecords = cameraRecords.getRecordsProvider([$scope.activeCamera.physicalId], 640);

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
                resolution = "320p";
            }*/

            if($scope.activeResolution == resolution){
                return;
            }
            $scope.activeResolution = resolution;
            updateVideoSource($scope.positionProvider.liveMode?null:$scope.positionProvider.playedPosition);
        };

        $scope.fullScreen = function(){
            if (screenfull.enabled) {
                screenfull.request($(".videowindow").get(0));
            }
        };





        function extractDomain(url) {
            url = url.split('/')[2] || url.split('/')[0];
            return url.split(':')[0].split('?')[0];
        }

        function getCameras() {

            var deferred = $q.defer();

            mediaserver.getCameras().then(function (data) {
                var cameras = data.data;


                var findMediaStream = function(param){
                    return param.name == "mediaStreams";
                };
                function cameraSorter(camera) {
                    camera.url = extractDomain(camera.url);
                    camera.preview = mediaserver.previewUrl(camera.physicalId, false, null, 256);
                    camera.server = getServer(camera.parentId);
                    if(camera.server.status == 'Offline'){
                        camera.status = 'Offline';
                    }

                    var mediaStreams = _.find(camera.addParams,findMediaStream);
                    camera.mediaStreams = mediaStreams?JSON.parse(mediaStreams.value).streams:[];

                    var num = 0;
                    var addrArray = camera.url.split('.');
                    for (var i = 0; i < addrArray.length; i++) {
                        var power = 3 - i;
                        num += ((parseInt(addrArray[i]) % 256 * Math.pow(256, power)));
                    }
                    if (isNaN(num)) {
                        num = camera.url;
                    }else {
                        num = num.toString(16);
                        if (num.length < 8) {
                            num = '0' + num;
                        }
                    }
                    return camera.name + '__' + num;
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
                //7 sort again
                cameras = _.sortBy(cameras, cameraSorter);

                //8. Group by servers
                $scope.cameras = _.groupBy(cameras, function (camera) {
                    return camera.parentId;
                });

                $scope.allcameras = cameras;

                deferred.resolve(cameras);
            }, function (error) {
                deferred.reject(error);
            });

            return deferred.promise;
        }

        function reloadTree(){

            var deferred = $q.defer();

            console.log("reloadTree");
            mediaserver.getMediaServers().then(function (data) {
                _.each(data.data, function (server) {
                    server.url = extractDomain(server.url);
                    var oldserver = getServer(server.id);
                    server.collapsed = oldserver? oldserver.collapsed : server.status !== 'Online' && (server.allowAutoRedundancy || server.flags.indexOf('SF_Edge') < 0);
                });
                $scope.mediaServers = data.data;

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
                $scope.selectCameraById($scope.cameraId,$location.search().time || false,firstTime);
                firstTime = false;
                timer = $timeout(reloader, reloadInterval);
            },function(error){
                console.error(error);
                timer = $timeout(reloader, quickReloadInterval);
            });
        }

        reloader();

        $scope.$on(
            '$destroy',
            function( ) {
                $timeout.cancel(timer);
            }
        );


        $rootScope.$on('$routeChangeStart', function (event, next/*, current*/) {
            $scope.selectCameraById(next.params.cameraId, $location.search().time || false);
        });


        (function (){
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
                var viewportHeight = (windowHeight - topHeight) + "px";

                $camerasPanel.css("height",viewportHeight );
                $viewPanel.css("height",viewportHeight );
            };
            updateHeights();
            $window.resize(updateHeights);
        })();
    });

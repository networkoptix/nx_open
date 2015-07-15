'use strict';

angular.module('webadminApp').controller('ViewCtrl',
    function ($scope,$rootScope,$location,$routeParams,mediaserver,cameraRecords,$sce) {

        $scope.cameras = {};
        $scope.activeCamera = null;

        $scope.activeResolution = '320p';
        $scope.availableResolutions = ['Auto', '1080p', '720p', '640p', '320p', '240p'];

        $scope.activeFormat = 'Auto';
        $scope.availableFormats = [
            'Auto',
            'video/webm',
            'video/mp4',
            'application/x-mpegURL',
            'video/MP2T',
            'video/3gpp',
            'video/x-motion-jpeg'
        ];

        $scope.settings = {id: null};
        mediaserver.getSettings().then(function (r) {
            $scope.settings = {
                id: r.data.reply.id
            };
        });

        function getCamerasServer(camera) {
            return _.find($scope.mediaServers, function (server) {
                return server.id === camera.parentId;
            });
        }

        function getServerUrl(server) {
            if ($scope.settings.id === server.id.replace('{', '').replace('}', '')) {
                return '';
            }
            return server.apiUrl.replace('http://', '').replace('https://', '');
        }

        function updateVideoSource(playing) {
            console.log("init positionProvider ", playing);
            var live = !playing;
            if(live){
                playing = (new Date()).getTime();
            }


            if(!$scope.positionProvider){
                return;
            }
            $scope.positionProvider.init(playing);
            var cameraId = $scope.activeCamera.physicalId;
            var server = getCamerasServer($scope.activeCamera);
            var serverUrl = '';


            if ($scope.settings.id && ($scope.settings.id.replace('{', '').replace('}', '') !== server.id.replace('{', '').replace('}', ''))) {
                serverUrl = '/proxy/' + getServerUrl(server);
            }


            var extParam = '';
            var mediaDemo = mediaserver.mediaDemo();
            if(mediaDemo){
                serverUrl = mediaDemo;
                extParam = Config.demoAuth;

            }



            var positionMedia = !live ? "&pos=" + (playing) : "";
            var positionHls = !live ? "&startTimestamp=" + (playing) : "";

            $scope.acitveVideoSource = _.filter([
                { src: ( serverUrl + '/media/' + cameraId + '.webm?resolution='   + $scope.activeResolution + positionMedia + extParam ), type: 'video/webm' },
                { src: ( serverUrl + '/media/' + cameraId + '.mp4?resolution='    + $scope.activeResolution + positionMedia + extParam ), type: 'video/mp4' },
                { src: ( serverUrl + '/hls/'   + cameraId + '.m3u8?resolution='   + $scope.activeResolution + positionHls   + extParam ), type: 'application/x-mpegURL'},
                { src: ( serverUrl + '/media/' + cameraId + '.mpegts?resolution=' + $scope.activeResolution + positionMedia + extParam ), type: 'video/MP2T'},
                { src: ( serverUrl + '/media/' + cameraId + '.3gp?resolution='    + $scope.activeResolution + positionMedia + extParam ), type: 'video/3gpp'},
                { src: ( serverUrl + '/media/' + cameraId + '.mpjpeg?resolution=' + $scope.activeResolution + positionMedia + extParam ), type: 'video/x-motion-jpeg'}
            ],function(src){
                return $scope.activeFormat == 'Auto'|| $scope.activeFormat == src.type;
            });

            console.log("video source", $scope.activeFormat, $scope.acitveVideoSource[0].src );


            console.log($scope.acitveVideoSource);
        }

        $scope.activeVideoRecords = null;
        $scope.positionProvider = null;

        $scope.updateTime = function(currentTime, duration){
            currentTime = currentTime || 0;

            $scope.positionProvider.setPlayingPosition(currentTime*1000);
            if(!$scope.positionProvider.liveMode) {
                $location.search('time', Math.round($scope.positionProvider.playedPosition));
            }
        };

        $scope.playerReady = function(API){
            $scope.playerAPI = API;
        };

        $scope.selectCameraById = function (cameraId,position) {

            $scope.cameraId = cameraId || $scope.cameraId;
            if(position){
                position = parseInt(position);
            }

            $scope.activeCamera = _.find($scope.allcameras, function (camera) {
                return camera.id === $scope.cameraId;
            });

            if ($scope.activeCamera) {
                $scope.positionProvider = cameraRecords.getPositionProvider([$scope.activeCamera.physicalId]);
                $scope.activeVideoRecords = cameraRecords.getRecordsProvider([$scope.activeCamera.physicalId], 640);

                updateVideoSource(position);

                $scope.switchPlaying(true);
            }
        };
        $scope.selectCamera = function (activeCamera) {
            $location.path('/view/' + activeCamera.id, false);
            $scope.selectCameraById(activeCamera.id,false);
        };

        $rootScope.$on('$routeChangeStart', function (event, next/*, current*/) {
            $scope.selectCameraById(next.params.cameraId, $location.search().time || false);
        });

        $scope.switchPlaying = function(play){
            if(play) {
                $scope.playerAPI.play();
            }else{
                $scope.playerAPI.pause();
            }
            if( $scope.positionProvider) {
                $scope.positionProvider.playing = play;
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

        function extractDomain(url) {
            url = url.split('/')[2] || url.split('/')[0];
            return url.split(':')[0].split('?')[0];
        }

        function getCameras() {
            mediaserver.getCameras().then(function (data) {
                var cameras = data.data;


                function cameraSorter(camera) {
                    camera.url = extractDomain(camera.url);
                    camera.preview = mediaserver.previewUrl(camera.physicalId, false, 70);

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

                //7 sort again
                cameras = _.sortBy(cameras, cameraSorter);

                //8. Group by servers
                $scope.cameras = _.groupBy(cameras, function (camera) {
                    return camera.parentId;
                });

                $scope.allcameras = cameras;
            }, function () {


                /*if(camera.id === $scope.cameraId){
                 $scope.selectCamera(camera);
                 }*/


                console.error('network problem');
            });
        }

        $scope.selectFormat = function(format){
            $scope.activeFormat = format;
            updateVideoSource($scope.positionProvider.playedPosition);
        };

        $scope.selectResolution = function(resolution){
            if(resolution == "auto" || resolution == "Auto" || resolution == "AUTO"){
                resolution = "320p";
            }

            if($scope.activeResolution == resolution){
                return;
            }
            $scope.activeResolution = resolution;
            updateVideoSource($scope.positionProvider.playedPosition);
        };

        $scope.fullScreen = function(){
            $scope.playerAPI.toggleFullScreen();
        };

        $scope.$watch('allcameras', function () {
            $scope.selectCameraById($scope.cameraId,$location.search().time || false);
        });

        mediaserver.getMediaServers().then(function (data) {
            _.each(data.data, function (server) {
                server.url = extractDomain(server.url);
                server.collapsed = server.status !== 'Online' && (server.allowAutoRedundancy || server.flags.indexOf('SF_Edge') < 0);
            });
            $scope.mediaServers = data.data;
            getCameras();

            $scope.selectCameraById($routeParams.cameraId,$location.search().time || false);
        }, function () {
            console.error('network problem');
        });


    });

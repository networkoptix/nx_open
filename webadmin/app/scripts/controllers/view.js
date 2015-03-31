'use strict';

angular.module('webadminApp').controller('ViewCtrl',
    function ($scope,$rootScope,$location,$routeParams,mediaserver,cameraRecords) {

        $scope.cameras = {};
        $scope.activeCamera = null;

        $scope.playingPosition = 0;
        $scope.activeResolution = '320p';
        $scope.availableResolutions = ['1080p', '720p', '640p', '320p', '240p'];

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

        function updateVideoSource() {
            var cameraId = $scope.activeCamera.physicalId;
            var server = getCamerasServer($scope.activeCamera);
            var serverUrl = '';
            if ($scope.settings.id !== server.id.replace('{', '').replace('}', '')) {
                serverUrl = '/proxy/' + getServerUrl(server);
            }
            //$scope.playingPosition
            var positionMedia = $scope.playingPosition>=0?"&pos=" + ($scope.playingPosition):"";
            var positionHls = $scope.playingPosition>=0?"&startTimestamp=" + ($scope.playingPosition):"";

            $scope.startPosition = $scope.playingPosition;

            $scope.acitveVideoSource = [
                { src: serverUrl + '/media/' + cameraId + '.webm?resolution=' + $scope.activeResolution + positionMedia, type: 'video/webm' },
                { src: serverUrl + '/media/' + cameraId + '.mp4?resolution=' + $scope.activeResolution+ positionMedia, type: 'video/mp4' },
                { src: serverUrl + '/hls/' + cameraId + '.m3u?resolution=' + $scope.activeResolution + positionHls},
                { src: serverUrl + '/hls/' + cameraId + '.m3u8?resolution=' + $scope.activeResolution + positionHls},
                { src: serverUrl + '/media/' + cameraId + '.mpegts?resolution=' + $scope.activeResolution + positionMedia},
                { src: serverUrl + '/media/' + cameraId + '.3gp?resolution=' + $scope.activeResolution + positionMedia},
                { src: serverUrl + '/media/' + cameraId + '.mpjpeg?resolution=' + $scope.activeResolution + positionMedia }
            ];
        }

        $scope.activeVideoRecords = null;

        $scope.updateTime = function(currentTime, duration){

            console.log("updateTime",currentTime, duration,this,arguments);
            currentTime = currentTime || 0;
            $scope.playingPosition = $scope.startPosition + currentTime*1000;
        };

        $scope.playerReady = function(API){
            console.log("playerReady",API,this,arguments);
            $scope.playerAPI = API;
        };

        $scope.selectCameraById = function (cameraId) {

            $scope.cameraId = cameraId || $scope.cameraId;

            $scope.activeCamera = _.find($scope.allcameras, function (camera) {
                return camera.id === $scope.cameraId;
            });

            if (!!$scope.activeCamera) {
                updateVideoSource();
                $scope.activeVideoRecords = cameraRecords.getRecordsProvider([$scope.activeCamera.physicalId]);
            }
        };
        $scope.selectCamera = function (activeCamera) {
            $location.path('/view/' + activeCamera.id, false);
            $scope.selectCameraById(activeCamera.id);
        };

        $rootScope.$on('$routeChangeStart', function (event, next/*, current*/) {
            $scope.selectCameraById(next.params.cameraId);
        });

        $scope.switchPosition = function( val ){
            console.log("switchPosition", val);

            $scope.playingPosition = val;
            updateVideoSource();
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

                    var num = 0;
                    try {
                        var addrArray = camera.url.split('.');
                        for (var i = 0; i < addrArray.length; i++) {
                            var power = 3 - i;
                            num += ((parseInt(addrArray[i]) % 256 * Math.pow(256, power)));
                        }
                        if (isNaN(num)) {
                            throw num;
                        }
                        num = num.toString(16);
                        if (num.length < 8) {
                            num = '0' + num;
                        }
                    } catch (a) {
                        num = camera.url;
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


                alert('network problem');
            });
        }

        $scope.$watch('allcameras', function () {
            $scope.selectCameraById();
        });

        mediaserver.getMediaServers().then(function (data) {
            _.each(data.data, function (server) {
                server.url = extractDomain(server.url);
                server.collapsed = server.status !== 'Online' && (server.allowAutoRedundancy || server.flags.indexOf('SF_Edge') < 0);
            });
            $scope.mediaServers = data.data;
            getCameras();
            $scope.selectCameraById($routeParams.cameraId);
        }, function () {
            alert('network problem');
        });


    });

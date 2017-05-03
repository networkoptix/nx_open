"use strict";
angular.module('webadminApp')
    .directive('cameraPanel', function ($rootScope,$location, mediaserver, cameraRecords, $poll, $q, $timeout) {
        return {
            restrict: 'E',
            scope: {
                currentUser: "=",
                activeCamera: "=",
                searchCams: "=",
                storage: "=",
                timeCorrection: "=",
                positionProvider: "=",
                updateVideoSource: "=",
                switchPlaying: "=",
                treeRequest: "=",
                activeVideoRecords: "=",
                liveOnly: "="
            },
            templateUrl: 'views/components/cameraPanel.html',
            link: function (scope, element/*, attrs*/) {

                scope.Config = Config;

                if(scope.currentUser === null ){
                    return; // Do nothing, user wasn't authorised
                }

                scope.cameras = {};

                var isAdmin = false;
                var canViewArchive = false;
                
                var reloadInterval = 5*1000;//30 seconds

                function getServer(id) {
                    if(!scope.mediaServers) {
                        return null;
                    }
                    return _.find(scope.mediaServers, function (server) {
                        return server.id === id;
                    });
                }
                function getCamera(id){
                    if(!scope.cameras) {
                        return null;
                    }
                    for(var serverId in scope.cameras) {
                        var cam = _.find(scope.cameras[serverId], function (camera) {
                            return camera.id === id || camera.physicalId === id;
                        });
                        if(cam){
                            return cam;
                        }
                    }
                    return null;
                }

                function updateStream(position){
                    scope.liveOnly = true;
                    if(canViewArchive) {
                        scope.activeVideoRecords.archiveReadyPromise.then(function (hasArchive) {
                            scope.liveOnly = !hasArchive;
                        });
                    }
                    scope.updateVideoSource(position);
                    scope.switchPlaying(true);
                }

                scope.selectCameraById = function (cameraId, position, silent) {
                    if(scope.activeCamera && (scope.activeCamera.id === cameraId || scope.activeCamera.physicalId === cameraId)){
                        return;
                    }

                    var oldTimePosition = null;
                    if(scope.positionProvider && !scope.positionProvider.liveMode){
                        oldTimePosition = scope.positionProvider.playedPosition;
                    }

                    scope.storage.cameraId  = cameraId || scope.storage.cameraId  ;

                    position = position?parseInt(position):oldTimePosition;

                    scope.activeCamera = getCamera (scope.storage.cameraId  );
                    if (!silent && scope.activeCamera) {
                        scope.positionProvider = cameraRecords.getPositionProvider([scope.activeCamera.physicalId], scope.timeCorrection);
                        scope.activeVideoRecords = cameraRecords.getRecordsProvider([scope.activeCamera.physicalId], 640, scope.timeCorrection);
                        $timeout(function(){updateStream(position)});
                    }
                };
                scope.selectCamera = function (activeCamera) {
                    $location.path('/view/' + activeCamera.id, false);
                    scope.selectCameraById(activeCamera.id,false);
                };
                scope.toggleServerCollapsed = function(server){
                    server.collapsed = !server.collapsed;
                    scope.storage.serverStates[server.id] = server.collapsed;
                };

                function searchCams(){
                    if(scope.searchCams.toLowerCase() == "links panel"){ // Enable cameras and clean serach fields
                        scope.cameraLinksEnabled = true;
                        scope.searchCams = "";
                    }
                    function has(str, substr){
                        return str && str.toLowerCase().indexOf(substr.toLowerCase()) >= 0;
                    }
                    _.forEach(scope.mediaServers,function(server){
                        var cameras = scope.cameras[server.id];
                        var camsVisible = false;
                        _.forEach(cameras,function(camera){
                            camera.visible = scope.searchCams === '' ||
                                    has(camera.name, scope.searchCams) ||
                                    has(camera.url, scope.searchCams);
                            camsVisible = camsVisible || camera.visible;
                        });

                        server.visible = scope.searchCams === '' ||
                            camsVisible /*||
                            has(server.name, scope.searchCams) ||
                            has(server.url, scope.searchCams)*/;
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

                function getCameras() {

                    var deferred = $q.defer();
                    if(scope.treeRequest){
                        scope.treeRequest.abort('getCameras');
                    }
                    scope.treeRequest = mediaserver.getCameras();
                    scope.treeRequest.then(function (data) {
                        var cameras = data.data;
                        var findMediaStream = function(param){
                            return param.name === 'mediaStreams';
                        };
                        
                        function cameraFilter(camera){
                            // Filter desktop cameras here
                            if(camera.typeId === desktopCameraTypeId){ // Hide desctop cameras
                                return false;
                            }

                            return true;
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

                        if(!scope.cameras) {
                            scope.cameras = camerasByServers;
                        }else{
                            for(var serverId in scope.cameras){
                                var activeCameras = scope.cameras[serverId];
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
                                if(!scope.cameras[serverId]){
                                    scope.cameras[serverId] = camerasByServers[serverId];
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
                        server.collapsed = scope.storage.serverStates[server.id];


                        if(typeof(server.visible) === 'undefined'){
                            server.visible = true;
                        }

                        return objectOrderName(server);
                    }

                    function newServerFilter(server){
                        var oldserver = getServer(server.id);

                        server.collapsed = oldserver ? oldserver.collapsed :
                            scope.storage.serverStates[server.id] ||
                            server.status !== 'Online' && (server.allowAutoRedundancy || server.flags.indexOf('SF_Edge') < 0);

                        if(oldserver){ // refresh old server
                            $.extend(oldserver,server);
                        }

                        return !oldserver;
                    }

                    var deferred = $q.defer();

                    if(scope.treeRequest){
                        scope.treeRequest.abort('reloadTree');
                    }
                    scope.treeRequest = mediaserver.getMediaServers();
                    scope.treeRequest.then(function (data) {

                        if(!scope.mediaServers) {
                            scope.mediaServers = _.sortBy(data.data,serverSorter);
                        }else{
                            var servers = data.data;
                            for(var i = 0; i < scope.mediaServers.length; i++) { // Remove old servers
                                if(!_.find(servers,function(server){
                                        return server.id === scope.mediaServers[i].id;
                                    }))
                                {
                                    scope.mediaServers.splice(i, 1);
                                    i--;
                                }
                            }


                            var newServers = _.filter(servers,newServerFilter); // Process all servers - refresh old and find new

                            _.forEach(_.sortBy(newServers,serverSorter),function(server){ // Add new server
                                scope.mediaServers.push(server);
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
                function reloader(){
                    return reloadTree().then(function(){
                        scope.selectCameraById(scope.storage.cameraId, firstTime && $location.search().time || false, !firstTime);
                        firstTime = false;
                    },function(error){
                        if(typeof(error.status) === 'undefined' || !error.status) {
                            console.error(error);
                        }
                    });
                }

                var poll = $poll(reloader,reloadInterval);
                scope.$on( '$destroy', function( ) {
                    killSubscription();
                    $poll.cancel(poll);
                });

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

                scope.$watch('searchCams',searchCams);

                mediaserver.getCurrentUser().then(function(result) {
                    isAdmin = result.data.reply.isAdmin || (result.data.reply.permissions.indexOf(Config.globalEditServersPermissions)>=0);
                    canViewArchive = result.data.reply.isAdmin || (result.data.reply.permissions.indexOf(Config.globalViewArchivePermission)>=0);

                    var userId = result.data.reply.id;
                    requestResourses(); //Show  whole tree
                });
                var killSubscription = $rootScope.$on('$routeChangeStart', function (event,next) {
                    scope.selectCameraById(next.params.cameraId, $location.search().time || false);
                });
            }   
        };
    });
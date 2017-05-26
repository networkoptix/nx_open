'use strict';
angular.module('nxCommon')
    .factory('camerasProvider', function (systemAPI, $localStorage, $q, $poll) {
        var reloadInterval = Config.webclient.reloadInterval;//30 seconds

        function camerasProvider(){
            this.desktopCameraTypeId = null;

            this.cameras = {};
            this.treeRequest = null;
            this.mediaServers = null;
            this.storage = $localStorage;
            this.poll = null;

            this.searchCams = null;
        }

        //setters
        camerasProvider.prototype.setSearchCams = function(searchCams){
            this.searchCams = searchCams;
        };

        //getters
        camerasProvider.prototype.getCamera = function(id){
            if(!this.cameras) {
                return null;
            }
            for(var serverId in this.cameras) {
                var cam = _.find(this.cameras[serverId], function (camera) {
                    return camera.id === id || camera.physicalId === id;
                });
                if(cam){
                    return cam;
                }
            }
            return null;
        };

        camerasProvider.prototype.getMediaServers = function(){
            return this.mediaServers;
        };

        //internal functions
        camerasProvider.prototype.extractDomain = function(url) {
            url = url.split('/')[2] || url.split('/')[0];
            return url.split(':')[0].split('?')[0];
        };

        camerasProvider.prototype.getServer = function(id) {
            if(this.mediaServers) {
                return null;
            }
            return _.find(this.mediaServers, function (server) {
                return server.id === id;
            });
        };

        camerasProvider.prototype.objectOrderName = function(object){
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
        };

        camerasProvider.prototype.getCameras = function() {
            var self = this;
            var deferred = $q.defer();
            if(this.treeRequest){
                this.treeRequest.abort('getCameras');
            }
            this.treeRequest = systemAPI.getCameras();
            this.treeRequest.then(function (data) {
                var cameras = data.data;
                var findMediaStream = function(param){
                    return param.name === 'mediaStreams';
                };
                
                function cameraFilter(camera){
                    // Filter desktop cameras here
                    if(camera.typeId === self.desktopCameraTypeId){ // Hide desctop cameras
                        return false;
                    }

                    return true;
                }

                function cameraSorter(camera) {
                    camera.url = self.extractDomain(camera.url);
                    camera.preview = systemAPI.previewUrl(camera.physicalId, false, null, 256);
                    camera.server = self.getServer(camera.parentId);
                    if(camera.server && camera.server.status === 'Offline'){
                        camera.status = 'Offline';
                    }

                    var mediaStreams = _.find(camera.addParams,findMediaStream);
                    camera.mediaStreams = mediaStreams?JSON.parse(mediaStreams.value).streams:[];
                    if(typeof(camera.visible) === 'undefined'){
                        camera.visible = true;
                    }

                    return self.objectOrderName(camera);
                }

                function newCamerasFilter(camera){
                    var oldCamera = self.getCamera(camera.id);
                    if(!oldCamera){
                        return true;
                    }

                    $.extend(oldCamera,camera);
                    return false;
                }
                /*

                 // self is for encoders (group cameras):

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

                if(!self.cameras) {
                    self.cameras = camerasByServers;
                }else{
                    for(var serverId in self.cameras){
                        var activeCameras = self.cameras[serverId];
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
                        if(!self.cameras[serverId]){
                            self.cameras[serverId] = camerasByServers[serverId];
                        }
                    }
                }

                deferred.resolve(cameras);
            }, function (error) {
                deferred.reject(error);
            });

            return deferred.promise;
        };

        camerasProvider.prototype.reloadTree = function(){
            var self = this;
            function serverSorter(server){
                server.url = self.extractDomain(server.url);
                server.collapsed = self.storage.serverStates[server.id];


                if(typeof(server.visible) === 'undefined'){
                    server.visible = true;
                }
                return self.objectOrderName(server);
            }

            function newServerFilter(server){
                var oldserver = self.getServer(server.id);

                server.collapsed = oldserver ? oldserver.collapsed :
                    self.storage.serverStates[server.id] ||
                    server.status !== 'Online' && (server.allowAutoRedundancy || server.flags.indexOf('SF_Edge') < 0);

                if(oldserver){ // refresh old server
                    $.extend(oldserver,server);
                }

                return !oldserver;
            }

            var deferred = $q.defer();
            if(this.treeRequest){
                this.treeRequest.abort('reloadTree');
            }
            this.treeRequest = systemAPI.getMediaServers();
            this.treeRequest.then(function (data) {
                if(!self.mediaServers) {
                    self.mediaServers = _.sortBy(data.data,serverSorter);
                }else{
                    var servers = data.data;
                    for(var i = 0; i < self.mediaServers.length; i++) { // Remove old servers
                        if(!_.find(servers,function(server){
                                return server.id === self.mediaServers[i].id;
                            }))
                        {
                            self.mediaServers.splice(i, 1);
                            i--;
                        }
                    }


                    var newServers = _.filter(servers,newServerFilter); // Process all servers - refresh old and find new

                    _.forEach(_.sortBy(newServers,serverSorter),function(server){ // Add new server
                        self.mediaServers.push(server);
                    });
                }
                self.getCameras().then(function(data){
                        self.searchCams();
                        deferred.resolve(data);
                    },
                    function(error){
                        self.searchCams();
                        deferred.reject(error);
                    });

            }, function (error) {
                deferred.reject(error);
            });

            return deferred.promise;
        };

        camerasProvider.prototype.requestResources = function() {
            var self = this;
            return systemAPI.getResourceTypes().then(function (result) {
                self.desktopCameraTypeId = _.find(result.data, function (type) {
                    return type.name === 'SERVER_DESKTOP_CAMERA';
                });
                self.desktopCameraTypeId = self.desktopCameraTypeId ? self.desktopCameraTypeId.id : null;
                return self.reloadTree();
            });
        };

        camerasProvider.prototype.startPoll = function(){
            this.poll = $poll(function(){
                this.reloadTree();
            },reloadInterval);
        };

        camerasProvider.prototype.stopPoll = function(){
            $poll.cancel(this.poll);
        };

        camerasProvider.prototype.abortTree = function(){
            if(this.treeRequest){
                this.treeRequest.abort('updateVideoSource'); //abort tree reloading request to speed up loading new video
            }
        };

        camerasProvider.prototype.getFirstCam = function(){
            var tmpServerList = this.mediaServers;
            var tmpCamerasList = this.cameras;
            for(var i in tmpServerList){
                var serverId = tmpServerList[i].id;
                for(var j in tmpCamerasList[serverId]){
                    if(tmpCamerasList[serverId][j] && tmpCamerasList[serverId][j].id){
                        return tmpCamerasList[serverId][j].id;
                    }
                }
            }
        };

        return {
            getProvider: function(){
                return new camerasProvider;
            }
        };
    });

"use strict";
angular.module('nxCommon')
    .directive('cameraPanel', ['$localStorage',function ($localStorage) {
        return {
            restrict: 'E',
            scope:{
                "activeCamera":"=",
                "camerasProvider": "=",
                "player": "=",
                "currentResolution": "=",
                "toggleCameraPanel": "="
            },
            templateUrl: Config.viewsDirCommon + 'components/cameraPanel.html',
            link: function (scope, element/*, attrs*/) {
                scope.Config = Config;
                scope.searchCams = "";
                scope.storage = $localStorage;

                var updateCameras = function(){
                    scope.cameras = scope.camerasProvider.cameras;
                };
                var updateMediaServers = function(){
                    scope.mediaServers = scope.camerasProvider.getMediaServers();
                };

                scope.camerasProvider.setSearchCams(searchCams);

                scope.$watch('camerasProvider.cameras',updateCameras);
                scope.$watch('camerasProvider.mediaServers',updateMediaServers);
                

                scope.selectCamera = function (activeCamera) {
                    scope.toggleCameraPanel = false;
                    if(scope.activeCamera && (scope.activeCamera.id === activeCamera.id || scope.activeCamera.physicalId === activeCamera)){
                        return;
                    }
                    scope.activeCamera = activeCamera;
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

                scope.$watch('searchCams',searchCams);
            }   
        };
    }]);

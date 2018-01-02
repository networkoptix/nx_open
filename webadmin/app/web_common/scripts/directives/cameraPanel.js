"use strict";
angular.module('nxCommon')
    .directive('cameraPanel', ['$localStorage',function ($localStorage) {
        return {
            restrict: 'E',
            scope:{
                "activeCamera":"=",
                "camerasProvider": "=",
                "showCameraPanel": "=",
                "searchCams": "="
            },
            templateUrl: Config.viewsDirCommon + 'components/cameraPanel.html',
            link: function (scope, element/*, attrs*/) {
                scope.Config = Config;
                scope.storage = $localStorage;
                scope.inputPlaceholder = L.common.searchCamPlaceholder;

                var updateCameras = function(){
                    scope.cameras = scope.camerasProvider.cameras;
                    searchCams();
                };
                var updateMediaServers = function(){
                    scope.mediaServers = scope.camerasProvider.getMediaServers();
                };

                scope.$watch('camerasProvider.cameras',updateCameras, true);
                scope.$watch('camerasProvider.mediaServers',updateMediaServers, true);
                

                scope.selectCamera = function (activeCamera) {
                    scope.showCameraPanel = false;
                    if(scope.activeCamera && (scope.activeCamera.id === activeCamera)){
                        return;
                    }
                    scope.activeCamera = activeCamera;
                };
                scope.toggleServerCollapsed = function(server){
                    server.collapsed = !server.collapsed;
                    scope.storage.serverStates[server.id] = server.collapsed;
                };
                
                function searchCams(){
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

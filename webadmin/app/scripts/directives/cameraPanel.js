"use strict";
angular.module('webadminApp')
    .directive('cameraPanel', function ($rootScope, $location, $poll) {
        return {
            restrict: 'E',
            scope:{
                "selectCameraById": "=",
                "camerasProvider": "="
            },
            templateUrl: 'views/components/cameraPanel.html',
            link: function (scope, element/*, attrs*/) {
                var reloadInterval = 5*1000;//30 seconds

                scope.Config = Config;
                scope.searchCams = "";

                var updateCameras = function(){
                    scope.cameras = scope.camerasProvider.cameras;
                };
                var updateMediaServers = function(){
                    scope.mediaServers = scope.camerasProvider.getMediaServers();
                };

                scope.camerasProvider.setSearchCams(searchCams);

                scope.$watch('camerasProvider.cameras',updateCameras);
                scope.$watch('camerasProvider.mediaServers',updateMediaServers);
                scope.camerasProvider.getResourses();
                

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

                scope.$watch('searchCams',searchCams);

                var poll = $poll(scope.camerasProvider.requestResources(),reloadInterval);
                scope.$on( '$destroy', function( ) {
                    killSubscription();
                    $poll.cancel(poll);
                });

                var killSubscription = $rootScope.$on('$routeChangeStart', function (event,next) {
                    scope.selectCameraById(next.params.cameraId, $location.search().time || false);
                });
            }   
        };
    });

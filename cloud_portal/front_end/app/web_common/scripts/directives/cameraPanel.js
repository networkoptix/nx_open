(function () {
    
    'use strict';

    function CameraPanel($localStorage, $timeout, $location, $routeParams,
                         nxConfigService, languageService) {
    
        const CONFIG = nxConfigService.getConfig();
        const LANG = languageService.lang;
        
        return {
            restrict   : 'E',
            scope      : {
                'activeCamera'   : '=',
                'camerasProvider': '=',
                'showCameraPanel': '=',
                'isEmbeded'      : '<',
                'showOnTop'      : '='
            },
            templateUrl: CONFIG.viewsDirCommon + 'components/cameraPanel.html',
            link       : function (scope/*, element, attrs*/) {
                scope.Config = CONFIG;
                scope.storage = $localStorage;
                scope.inputPlaceholder = LANG.common.searchCamPlaceholder;
                
                scope.panel = {
                    filter : ''
                };
                
                scope.selectCamera = function (activeCamera) {
                    if (scope.activeCamera && (scope.activeCamera.id === activeCamera.id)) {
                        return;
                    }
                    scope.activeCamera = activeCamera;
                };
    
                function searchCameras() {
                    function has(str, substr) {
                        return str && str.toLowerCase().replace(/\s/g, '').indexOf(substr.toLowerCase().replace(/\s/g, '')) >= 0;
                    }
    
                    angular.forEach(scope.mediaServers, function (server) {
                        var cameras = scope.cameras[server.id];
                        var camsVisible = false;
    
                        angular.forEach(cameras, function (camera) {
                            camera.visible = scope.panel.filter === '' ||
                                has(camera.name, scope.panel.filter) ||
                                has(camera.url, scope.panel.filter);
                            camsVisible = camsVisible || camera.visible;
                        });
            
                        server.visible = scope.panel.filter === '' || camsVisible /*||
                        has(server.name, scope.panel.filter) ||
                        has(server.url, scope.panel.filter)*/;
                    });
                }
                
                function updateCameras () {
                    scope.cameras = scope.camerasProvider.cameras;

                    scope.toggleInfo = scope.storage.infoStatus;
                    var selected = false;
                    for (var serverId in scope.cameras) {
                        if (scope.cameras.hasOwnProperty(serverId)) {

                            for (var idx = 0; idx < scope.cameras[serverId].length; idx++) {
                                if (scope.isEmbeded && scope.cameras[serverId][idx].id === '{' + $routeParams.cameraId + '}' ||
                                    !scope.isEmbeded && scope.cameras[serverId][idx].id === scope.storage.activeCameras[serverId]) {
                                    
                                    scope.selectCamera(scope.cameras[serverId][idx]);
                                    selected = true;
                                    break;
                                }
                            }

                            if (selected) {
                                break;
                            }
                        }
                    }

                    if (!selected) {
                        if (Object.keys(scope.cameras).length !== 0 && !!scope.mediaServers) {
                            scope.selectCamera(scope.cameras[scope.mediaServers[0].id][0]);
                            scope.storage.serverStates[scope.mediaServers[0].id] = true; // expand default
                            scope.mediaServers[0].expanded = true;
                            scope.toggleInfo = false;
                            scope.storage.infoStatus = scope.toggleInfo;
                        }
                    }

                    searchCameras();
                }

                scope.toggleInfoFn = function () {
                    scope.toggleInfo = !scope.toggleInfo;
                    scope.storage.infoStatus = scope.toggleInfo;
                };

                scope.showCamera = function (camera) {
                    scope.showOnTop = false;
                    scope.selectCamera(camera);
                };

                scope.toggleServerCollapsed = function (server) {
                    server.expanded = !server.expanded;
                    scope.storage.serverStates[ server.id ] = server.expanded;
                };

                function updateMediaServers () {
                    scope.mediaServers = scope.camerasProvider.getMediaServers();

                    if (scope.mediaServers) {
                        scope.mediaServers.forEach(function (server) {
                            if (Object.keys(scope.storage.serverStates).length === 0) {
                                server.expanded = false;
                                return;
                            }

                            server.expanded = scope.storage.serverStates[server.id] !== undefined && scope.storage.serverStates[server.id];
                        });
                    }
                }
                
                scope.$watch('panel.filter', searchCameras);
                scope.$watch('camerasProvider.cameras', updateCameras, true);
                scope.$watch('camerasProvider.mediaServers', updateMediaServers, true);
            }
        };
    }
    
    CameraPanel.$inject = ['$localStorage', '$timeout', '$location', '$routeParams', 'nxConfigService', 'languageService'];
    
    angular
        .module('nxCommon')
        .directive('cameraPanel', CameraPanel);
})();

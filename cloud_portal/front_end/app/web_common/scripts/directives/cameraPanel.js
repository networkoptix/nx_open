(function () {
    "use strict";

    angular
        .module('nxCommon')
        .directive('cameraPanel', CameraPanel);

    CameraPanel.$inject = [ '$localStorage', '$timeout', '$filter' ];

    function CameraPanel($localStorage, $timeout, $filter) {

        return {
            restrict   : 'E',
            scope      : {
                "activeCamera"   : "=",
                "camerasProvider": "=",
                "showCameraPanel": "=",
                "searchCams"     : "="
            },
            templateUrl: Config.viewsDirCommon + 'components/cameraPanel.html',
            link       : function (scope/*, element, attrs*/) {
                scope.Config = Config;
                scope.storage = $localStorage;
                scope.inputPlaceholder = L.common.searchCamPlaceholder;

                scope.selectCamera = function (activeCamera) {
                    if (scope.activeCamera && (scope.activeCamera.id === activeCamera)) {
                        return;
                    }
                    scope.activeCamera = activeCamera;
                };

                function updateCameras () {
                    scope.cameras = scope.camerasProvider.cameras;

                    scope.toggleInfo = scope.storage.infoStatus;
                    var selected = false;
                    for (var serverId in scope.cameras) {
                        if (scope.cameras.hasOwnProperty(serverId)) {

                            for (var idx = 0; idx < scope.cameras[serverId].length; idx++) {
                                if (scope.cameras[serverId][idx].id === scope.storage.activeCameras[serverId]) {
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

                    searchCams();
                }

                scope.toggleInfoFn = function () {
                    scope.toggleInfo = !scope.toggleInfo;
                    scope.storage.infoStatus = scope.toggleInfo;
                };

                scope.showCamera = function (camera) {
                    scope.showCameraPanel = false;
                    scope.selectCamera(camera);
                };

                scope.toggleServerCollapsed = function (server) {
                    server.expanded = !server.expanded;
                    scope.storage.serverStates[ server.id ] = server.expanded;
                };

                function searchCams(searchText) {
                    function has(str, substr) {
                        return str && str.toLowerCase().replace(/\s/g, '').indexOf(substr.toLowerCase().replace(/\s/g, '')) >= 0;
                    }

                    //If the text is blank allow scope.searchCams to update in dom then update cameras
                    if (searchText === '') {
                        return $timeout(searchCams);
                    }

                    _.forEach(scope.mediaServers, function (server) {
                        var cameras = scope.cameras[ server.id ];
                        var camsVisible = false;

                        _.forEach(cameras, function (camera) {
                            camera.visible = scope.searchCams === '' ||
                                has(camera.name, scope.searchCams) ||
                                has(camera.url, scope.searchCams);
                            camsVisible = camsVisible || camera.visible;
                        });

                        server.visible = scope.searchCams === '' || camsVisible /*||
                        has(server.name, scope.searchCams) ||
                        has(server.url, scope.searchCams)*/;
                    });
                }

                function updateMediaServers () {
                    scope.mediaServers = scope.camerasProvider.getMediaServers();

                    if (scope.mediaServers) {
                        scope.mediaServers.forEach(function (server) {
                            if (Object.keys(scope.storage.serverStates).length === 0) {
                                server.expanded = false;
                                return;
                            }

                            server.expanded = scope.storage.serverStates[server.id] !== undefined && scope.storage.serverStates[server.id];
                        })
                    }
                }

                scope.$watch('searchCams', searchCams);
                scope.$watch('camerasProvider.cameras', updateCameras, true);
                scope.$watch('camerasProvider.mediaServers', updateMediaServers, true);
            }
        };
    }
})();

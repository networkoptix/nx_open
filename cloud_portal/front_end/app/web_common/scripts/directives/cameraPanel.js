(function () {
    "use strict";

    angular
        .module('nxCommon')
        .directive('cameraPanel', CameraPanel);

    CameraPanel.$inject = [ '$localStorage', '$timeout', '$filter' ];

    function CameraPanel($localStorage, $timeout, $filter) {

        function init(scope) {
            scope.mediaServers.forEach(function (server) {
                if (Object.keys(scope.storage.serverStates).length === 0) {
                    server.collapsed = true;
                    return;
                }

                server.collapsed = false; // initialize
                server.collapsed = scope.storage.serverStates[ server.id ] !== undefined && scope.storage.serverStates[ server.id ];
            })
        }

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

                var updateCameras = function () {
                    scope.cameras = scope.camerasProvider.cameras;

                    if (Object.keys(scope.storage.serverStates).length === 0) {
                        if (Object.keys(scope.cameras).length !== 0 && scope.mediaServers !== undefined) {
                            scope.selectCamera(scope.cameras[ scope.mediaServers[ 0 ].id ][ 0 ]);

                            scope.toggleInfo = false;
                            scope.storage.infoStatus = scope.toggleInfo;
                        }
                    } else {
                        var selected = false;
                        for (var server in scope.cameras) {
                            if (scope.cameras.hasOwnProperty(server)) {

                                for (var idx = 0; idx < scope.cameras[ server ].length; idx++) {
                                    if (scope.cameras[ server ][ idx ].id === scope.storage.cameraId) {
                                        scope.selectCamera(scope.cameras[ server ][ idx ]);
                                        selected = true;
                                        return;
                                    }
                                }

                                if (selected) {
                                    return;
                                }
                            }
                        }
                    }

                    searchCams();
                };

                var updateMediaServers = function () {
                    scope.mediaServers = scope.camerasProvider.getMediaServers();
                    init(scope);
                };

                scope.$watch('camerasProvider.cameras', updateCameras, true);
                scope.$watch('camerasProvider.mediaServers', updateMediaServers, true);

                scope.toggleInfoFn = function () {
                    scope.toggleInfo = !scope.toggleInfo;
                    scope.storage.infoStatus = scope.toggleInfo;
                };

                scope.selectCamera = function (activeCamera) {
                    scope.showCameraPanel = false;
                    if (scope.activeCamera && (scope.activeCamera.id === activeCamera)) {
                        return;
                    }
                    scope.activeCamera = activeCamera;
                };
                scope.toggleServerCollapsed = function (server) {
                    server.collapsed = !server.collapsed;
                    scope.storage.serverStates[ server.id ] = server.collapsed;
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

                scope.$watch('searchCams', searchCams);
            }
        };
    }
})();

(function () {

    'use strict';

    angular
        .module('cloudApp')
        .controller('ViewPageCtrl', [ '$rootScope', '$scope', 'account', 'system', '$routeParams', 'systemAPI', 'dialogs',
            '$location', '$q', '$poll', 'authorizationCheckService', 'camerasProvider',

            function ($rootScope, $scope, account, system, $routeParams, systemAPI, dialogs,
                      $location, $q, $poll, authorizationCheckService, camerasProvider) {

                $scope.systemReady = false;
                $scope.hasCameras = false;

                authorizationCheckService
                    .requireLogin()
                    .then(function (account) {
                        $scope.currentSystem = system($routeParams.systemId, account.email);
                        var systemInfoRequest = $scope.currentSystem.getInfo();
                        var systemAuthRequest = $scope.currentSystem.updateSystemAuth();

                        $q.all([ systemInfoRequest, systemAuthRequest ]).then(function () {
                            $scope.system = $scope.currentSystem.mediaserver;
                            delayedUpdateSystemInfo();

                            $scope.hasCameras = false;
                            $scope.system.getCameras().then(function (cameras) {
                                if (cameras.data.length > 0) {
                                    $scope.hasCameras = true;
                                    $scope.systemReady = true;
                                }
                            });

                            // Set footer visibility according to system status
                            $rootScope.$emit('nx.layout.footer', $scope.currentSystem.isOnline);

                        }, function () {
                            dialogs.notify(L.errorCodes.lostConnection.replace("{{systemName}}",
                                $scope.currentSystem.name || L.errorCodes.thisSystem), 'warning');
                            $location.path("/systems");
                        });
                    });

                function delayedUpdateSystemInfo() {
                    var pollingSystemUpdate = $poll(function () {
                        return $scope.currentSystem.update();
                    }, Config.updateInterval);

                    $scope.$on('$destroy', function (event) {
                        $poll.cancel(pollingSystemUpdate);
                    });
                }

                var cancelSubscription = $scope.$on("unauthorized_" + $routeParams.systemId, function (event, data) {
                    dialogs.notify(L.errorCodes.lostConnection.replace("{{systemName}}",
                        $scope.currentSystem.info.name || L.errorCodes.thisSystem), 'warning');
                    $location.path("/systems");
                });

                $scope.$on('$destroy', function (event) {
                    cancelSubscription();
                    // Reset footer visibility state
                    $rootScope.$emit('nx.layout.footer', false);
                });
            } ]);
})();

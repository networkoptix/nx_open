(function () {

    'use strict';

    angular
        .module('cloudApp')
        .controller('ViewPageCtrl', [ '$rootScope', '$scope', 'account', 'system', '$routeParams', 'systemAPI', 'dialogs',
            '$location', '$q', '$poll', 'authorizationCheckService',

            function ($rootScope, $scope, account, system, $routeParams, systemAPI, dialogs, $location, $q, $poll, authorizationCheckService) {
                authorizationCheckService
                    .requireLogin()
                    .then(function (account) {
                        $scope.currentSystem = system($routeParams.systemId, account.email);
                        var systemInfoRequest = $scope.currentSystem.getInfo();
                        var systemAuthRequest = $scope.currentSystem.updateSystemAuth();
                        $q.all([ systemInfoRequest, systemAuthRequest ]).then(function () {
                            $scope.systemReady = true;
                            $scope.system = $scope.currentSystem.mediaserver;
                            delayedUpdateSystemInfo();

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
                        pollingSystemUpdate.cancel();
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

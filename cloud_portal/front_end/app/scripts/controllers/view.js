(function () {

    'use strict';

    angular
        .module('cloudApp')
        .controller('ViewPageCtrl', [ '$rootScope', '$scope', '$window', 'account', 'system', '$routeParams', 'systemAPI', 'dialogs',
            '$location', '$q', '$poll', 'authorizationCheckService', 'camerasProvider',
            'nxConfigService', 'languageService',

            function ($rootScope, $scope, $window, account, system, $routeParams, systemAPI, dialogs,
                      $location, $q, $poll, authorizationCheckService, camerasProvider,
                      nxConfigService, languageService) {
    
                const CONFIG = nxConfigService.getConfig();
                const LANG = languageService.lang;
            
                $scope.systemReady = false;
                $scope.hasCameras = false;
    
                // Check if page is displayed inside an iframe
                $scope.isInIframe = ($window.location !== $window.parent.location);
    
                function delayedUpdateSystemInfo() {
                    var pollingSystemUpdate = $poll(function () {
                        return $scope.currentSystem.update();
                    }, CONFIG.updateInterval);
        
                    $scope.$on('$destroy', function () {
                        $poll.cancel(pollingSystemUpdate);
                    });
                }

                $rootScope.$emit('nx.layout.footer', {
                    state: true, // hide it
                    loc  : 'ViewPageCtrl - offline'
                });
                
                // Check if page is displayed inside an iframe
                $scope.isInIframe = ($window.location !== $window.parent.location);
                
                if ($scope.isInIframe) {
                    $rootScope.$emit('nx.layout.header', {
                        state: true, // hide it
                        loc: 'ViewPageCtrl - inIframe'
                    });
                    $rootScope.$emit('nx.layout.footer', {
                        state: true, // hide it
                        loc: 'ViewPageCtrl - inIframe'
                    });
                }
                
                function systemError(){
                    dialogs.notify(LANG.errorCodes.lostConnection.replace('{{systemName}}',
                        $scope.currentSystem.name || LANG.errorCodes.thisSystem), 'warning');
    
                    if ($scope.isInIframe) {
                        $location.path(CONFIG.viewsDir + '404.html');
                    } else {
                        $location.path('/systems');
                    }
                }
                authorizationCheckService
                    .requireLogin()
                    .then(function (account) {
                        $scope.currentSystem = system($routeParams.systemId, account.email);
                        var systemInfoRequest = $scope.currentSystem.getInfo();
                        var systemAuthRequest = $scope.currentSystem.updateSystemAuth();

                        $q.all([ systemInfoRequest, systemAuthRequest ]).then(function () {
                            $scope.system = $scope.currentSystem.mediaserver;

                            $scope.hasCameras = false;

                            if ($scope.currentSystem.isOnline) {
                                $scope.camerasProvider = camerasProvider.getProvider($scope.system);
                                $scope.camerasProvider
                                    .requestResources()
                                    .then(function () {
                                        $scope.system.getCameras().then(function (cameras) {
                                            $scope.camerasProvider.getCameras(cameras.data);
                                            $scope.systemReady = true;
                                            $scope.hasCameras = (Object.keys($scope.camerasProvider.cameras).length);

                                            if ($scope.hasCameras) {
                                                delayedUpdateSystemInfo();
                                            }
                                        }, systemError);
                                    }, systemError);
                            } else {
                                $scope.systemReady = true;
                            }
                        }, systemError);
                    });

                

                var cancelSubscription = $scope.$on('unauthorized_' + $routeParams.systemId, function () {
                    dialogs.notify(LANG.errorCodes.lostConnection.replace('{{systemName}}',
                        $scope.currentSystem.info.name || LANG.errorCodes.thisSystem), 'warning');
    
                    if ($scope.isInIframe) {
                        $location.path(CONFIG.viewsDir + '404.html');
                    } else {
                        $location.path('/systems');
                    }
                });

                $scope.$on('$destroy', function () {
                    cancelSubscription();
                    // Reset visibility state
                    if (!$scope.isInIframe) {
                        $rootScope.$emit('nx.layout.header', {state: false});
                        $rootScope.$emit('nx.layout.footer', {state: false});
                    }
                });
            }]);
})();

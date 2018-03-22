(function () {

    'use strict';

    angular
        .module('cloudApp')
        .directive('nxHeader', NxHeader);

    NxHeader.$inject = [ 'nxDialogsService', 'cloudApi', 'account', '$location', '$route',
        'systemsProvider', 'CONFIG' ];

    function NxHeader(nxDialogsService, cloudApi, account, $location, $route,
                      systemsProvider, CONFIG) {
        return {
            restrict   : 'E',
            templateUrl: CONFIG.viewsDir + 'components/header.html',
            link       : function (scope) {
                scope.config = CONFIG;
                scope.inline = typeof($location.search().inline) != 'undefined';

                if (scope.inline) {
                    $('body').addClass('inline-portal');
                }

                scope.login = function () {
                    nxDialogsService.login();
                };
                scope.logout = function () {
                    account.logout();
                };

                scope.systemsProvider = systemsProvider;
                scope.$watch('systemsProvider.systems', function () {
                    scope.systems = scope.systemsProvider.systems;
                    scope.singleSystem = (scope.systems.length == 1);
                    scope.systemCounter = scope.systems.length;
                    updateActiveSystem();
                });


                function isActive(val) {
                    var currentPath = $location.path();
                    if (currentPath.indexOf(val) < 0) { // no match
                        return false;
                    }
                    return true;
                }

                scope.active = {};

                function updateActive() {
                    scope.active.register = isActive('/register');
                    scope.active.view = isActive('/view');
                    scope.active.settings = $route.current.params.systemId && !isActive('/view');
                }

                function updateActiveSystem() {
                    if (!scope.systems) {
                        return;
                    }
                    scope.activeSystem = _.find(scope.systems, function (system) {
                        return $route.current.params.systemId == system.id;
                    });
                    if (scope.singleSystem) { // Special case for a single system - it always active
                        scope.activeSystem = scope.systems[ 0 ];
                    }
                }

                updateActive();
                account.get().then(function (account) {
                    scope.account = account;

                    $('body').removeClass('loading');
                    $('body').addClass('authorized');
                }, function () {
                    $('body').removeClass('loading');
                    $('body').addClass('anonymous');
                });


                scope.$on('$locationChangeSuccess', function (next, current) {
                    if ($route.current.params.systemId) {
                        if (!scope.systems) {
                            scope.systemsProvider.forceUpdateSystems();
                        } else {
                            updateActiveSystem();
                        }
                    }
                    updateActive();
                });

            }
        };
    }
})();

(function () {

    'use strict';

    angular
        .module('cloudApp.services')
        .service('locationProxyService', LocationProxyService);

    LocationProxyService.$inject = [ '$location', '$rootScope', '$route', 'nxConfigService' ];

    function LocationProxyService($location, $rootScope, $route, nxConfigService) {

        const CONFIG = nxConfigService.getConfig();

        // Support changing location without reloading controller
        const original = $location.path;
        $location.path = (path, reload) => {
            if (reload === false) {
                if (original.apply($location) == path) {
                    return;
                }

                let routeToKeep = $route.current;
                let unsubscribe = $rootScope.$on('$locationChangeSuccess', () => {
                    if (routeToKeep) {
                        $route.current = routeToKeep;
                        routeToKeep = null;
                    }
                    unsubscribe();
                    unsubscribe = null;
                });
            }
            if ($location.search().debug) {
                CONFIG.allowDebugMode = $location.search().debug;
            }
            if ($location.search().beta) {
                CONFIG.allowBetaMode = $location.search().beta;
            }
            return original.apply($location, [ path ]);
        };

        return {
            path: $location.path
        };
    }
})();

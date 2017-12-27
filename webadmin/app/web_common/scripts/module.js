angular.module('nxCommon', [
    'ngStorage'
]).config(['$compileProvider', function($compileProvider) {
    $compileProvider.aHrefSanitizationWhitelist(/^\s*(https?|rtsp|tel|mailto):/);
}]).run(['$route', '$rootScope', '$location', 'page', '$localStorage', function ($route, $rootScope, $location, page, $localStorage) {

    // Support changing location without reloading controller
    var original = $location.path;
    $location.path = function (path, reload) {
        if(reload === false) {
            if (original.apply($location) == path) return;

            var routeToKeep = $route.current;
            var unsubscribe = $rootScope.$on('$locationChangeSuccess', function () {
                if (routeToKeep) {
                    $route.current = routeToKeep;
                    routeToKeep = null;
                }
                unsubscribe();
                unsubscribe = null;
            });
        }
        if($location.search().debug){
            Config.allowDebugMode = $location.search().debug;
        }
        if($location.search().cast){
            Config.allowCastMode = $location.search().cast && window.chrome;
        }
        return original.apply($location, [path]);
    };

    // Support changing page titles
    $rootScope.$on('$routeChangeSuccess', function (event, current, previous) {
        if(current.$$route){
            page.title(current.$$route.title);
        }else{
            page.title(L.pageTitles.pageNotFound);
        }
    });

    // Add localstorage to rootScope to support global updates
    $rootScope.storage = $localStorage;


    // Expose Language and Config for all templates
    $rootScope.C = Config;
    $rootScope.L = L;
}]);
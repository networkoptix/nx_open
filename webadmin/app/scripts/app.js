'use strict';

angular.module('webadminApp', [
    'ipCookie',
    'ngResource',
    'ngSanitize',
    'ngRoute',
    'ui.bootstrap',
    'tc.chartjs',


    'com.2fdevs.videogular',
    'com.2fdevs.videogular.plugins.controls',
    'com.2fdevs.videogular.plugins.buffering',
    'com.2fdevs.videogular.plugins.dash',
    'com.2fdevs.videogular.plugins.poster'


]).config(function ($routeProvider) {
    $routeProvider
        .when('/settings', {
            templateUrl: 'views/settings.html',
            controller: 'SettingsCtrl'
        })
        .when('/join', {
            templateUrl: 'views/join.html',
            controller: 'SettingsCtrl'
        })
        .when('/info', {
            templateUrl: 'views/info.html',
            controller: 'InfoCtrl'
        })
        .when('/developers', {
            templateUrl: 'views/developers.html',
            controller: 'MainCtrl'
        })
        .when('/support', {
            templateUrl: 'views/support.html',
            controller: 'MainCtrl'
        })
        .when('/help', {
            templateUrl: 'views/help.html',
            controller: 'MainCtrl'
        })
        .when('/login', {
            templateUrl: 'views/login.html'
        })
        .when('/advanced', {
            templateUrl: 'views/advanced.html',
            controller: 'AdvancedCtrl'
        })
        .when('/webclient', {
            templateUrl: 'views/webclient.html',
            controller: 'WebclientCtrl',
            reloadOnSearch: false
        }).when('/view/', {
            templateUrl: 'views/view.html',
            controller: 'ViewCtrl',
            reloadOnSearch: false
        }).when('/view/:cameraId', {
            templateUrl: 'views/view.html',
            controller: 'ViewCtrl',
            reloadOnSearch: false
        })
        .when('/sdkeula', {
            templateUrl: 'views/sdkeula.html',
            controller: 'SdkeulaCtrl'
        })
        .when('/log', {
            templateUrl: 'views/log.html',
            controller: 'LogCtrl'
        })
        .when('/log', {
            templateUrl: 'views/log.html',
            controller: 'LogCtrl'
        })
        .otherwise({
            redirectTo: '/settings'
        });
});


angular.module('webadminApp').run(['$route', '$rootScope', '$location', function ($route, $rootScope, $location) {
    var original = $location.path;
    $location.path = function (path, reload) {
        if (reload === false) {
            var lastRoute = $route.current;
            var un = $rootScope.$on('$locationChangeSuccess', function () {
                $route.current = lastRoute;
                un();
            });
        }
        return original.apply($location, [path]);
    };
}]);
'use strict';

angular.module('webadminApp', [
    'ngResource',
    'ngSanitize',
    'ngRoute',
    //'ngTouch',
    'ui.bootstrap',
    'tc.chartjs',
    'ngStorage',
    'typeahead-focus'
]).config(function ($routeProvider) {

    var universalResolves = {
        currentUser: ['mediaserver',function(mediaserver){
            return mediaserver.resolveNewSystemAndUser();
        }]
    };

    var customRouteProvider = angular.extend({}, $routeProvider, {
        when: function(path, route) {
            route.resolve = (route.resolve) ? route.resolve : {};
            angular.extend(route.resolve, universalResolves);
            $routeProvider.when(path, route);
            return this;
        },
        otherwise:function(route){
            $routeProvider.otherwise( route);
        }
    });

    customRouteProvider
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
        .when('/debug', {
            templateUrl: 'views/debug.html',
            controller: 'DebugCtrl'
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
        .when('/sdkeula/:sdkFile', {
            templateUrl: 'views/sdkeula.html',
            controller: 'SdkeulaCtrl'
        })
        .when('/log', {
            templateUrl: 'views/log.html',
            controller: 'LogCtrl'
        })
        .when('/setup', {
            templateUrl: 'views/dialogs/setup.html',
            controller: 'SetupCtrl'
        })
        .otherwise({
            redirectTo: '/view'
        });
}).run(['$route', '$rootScope', '$location', function ($route, $rootScope, $location) {
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
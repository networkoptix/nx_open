'use strict';

angular.module('webadminApp', [
    'ngResource',
    'ngSanitize',
    'ngRoute',
    //'ngTouch',
    'ui.bootstrap',
    'tc.chartjs',
    'ngStorage',
    'typeahead-focus',
    'ui.timepicker'
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
        .when('/settings/system', {
            templateUrl: 'views/settings.html',
            controller: 'SettingsCtrl'
        })
        .when('/settings/server', {
            templateUrl: 'views/settings.html',
            controller: 'SettingsCtrl'
        })
        .when('/settings/device', {
            templateUrl: 'views/settings.html',
            controller: 'SettingsCtrl'
        })
        .when('/settings', {
            redirectTo: '/settings/server'
        })
        .when('/join', {
            templateUrl: 'views/join.html',
            controller: 'SettingsCtrl'
        })
        .when('/info', {
            templateUrl: 'views/info.html',
            controller: 'InfoCtrl'
        })
        .when('/developers/changelog', {
            templateUrl: 'views/api_changelog.html',
            controller: 'MainCtrl'
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
        .when('/client', {
            templateUrl: 'views/client.html',
            controller: 'ClientCtrl',
            reloadOnSearch: false
        })
        .when('/view', {
            templateUrl: 'views/view.html',
            controller: 'ViewCtrl',
            reloadOnSearch: false
        })
        .when('/view/:cameraId', {
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
        .when('/setup', {
            templateUrl: 'views/dialogs/setup.html',
            controller: 'SetupCtrl'
        })
        .when('/', {
            template: '',
            controller: 'MainCtrl'
        })
        .otherwise({
            redirectTo: '/'
        });
}).run(['$route', '$rootScope', '$location', function ($route, $rootScope, $location, $localStorage) {
    var original = $location.path;
    $rootScope.storage = $localStorage;
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
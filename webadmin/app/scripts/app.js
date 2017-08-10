'use strict';

angular.module('webadminApp', [
    'nxCommon',
    'ngResource',
    'ngSanitize',
    'ngRoute',
    //'ngTouch',
    'ui.bootstrap',
    'tc.chartjs',
    'ngStorage',
    'typeahead-focus',
    'ui.timepicker',
    'angular-clipboard'
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
            templateUrl: Config.viewsDir + 'settings.html',
            controller: 'SettingsCtrl'
        })
        .when('/settings/server', {
            templateUrl: Config.viewsDir + 'settings.html',
            controller: 'SettingsCtrl'
        })
        .when('/settings/device', {
            templateUrl: Config.viewsDir + 'settings.html',
            controller: 'SettingsCtrl'
        })
        .when('/settings', {
            redirectTo: '/settings/server'
        })
        .when('/join', {
            templateUrl: Config.viewsDir + 'join.html',
            controller: 'SettingsCtrl'
        })
        .when('/info', {
            templateUrl: Config.viewsDir + 'info.html',
            controller: 'InfoCtrl'
        })
        .when('/developers/changelog', {
            templateUrl: Config.viewsDir + 'api_changelog.html',
            controller: 'MainCtrl'
        })
        .when('/support', {
            templateUrl: Config.viewsDir + 'support.html',
            controller: 'MainCtrl'
        })
        .when('/help', {
            templateUrl: Config.viewsDir + 'help.html',
            controller: 'MainCtrl'
        })
        .when('/login', {
            templateUrl: Config.viewsDir + 'login.html'
        })
        .when('/advanced', {
            templateUrl: Config.viewsDir + 'advanced.html',
            controller: 'AdvancedCtrl'
        })
        .when('/debug', {
            templateUrl: Config.viewsDir + 'debug.html',
            controller: 'DebugCtrl'
        })
        .when('/developers', {
            templateUrl: Config.viewsDir + 'devtools/developers.html',
            controller: 'DevtoolsCtrl'
        })
        .when('/developers/api/:apiMethod*', {
            templateUrl: Config.viewsDir + 'devtools/api.html',
            controller: 'ApiToolCtrl'
        })
        .when('/developers/api', {
            templateUrl: Config.viewsDir + 'devtools/api.html',
            controller: 'ApiToolCtrl'
        })
        .when('/developers/events', {
            templateUrl: Config.viewsDir + 'devtools/events.html',
            controller: 'DevtoolsCtrl'
        })
        .when('/client', {
            templateUrl: Config.viewsDir + 'client.html',
            controller: 'ClientCtrl',
            reloadOnSearch: false
        })
        .when('/view', {
            templateUrl: Config.viewsDir + 'view.html',
            reloadOnSearch: false
        })
        .when('/view/:cameraId', {
            templateUrl: Config.viewsDir + 'view.html',
            reloadOnSearch: false
        })
        .when('/viewdebug', {
            templateUrl: Config.viewsDir + 'viewdebug.html',
            controller: 'ViewdebugCtrl',
            reloadOnSearch: false
        })
        .when('/viewdebug/:cameraId', {
            templateUrl: Config.viewsDir + 'viewdebug.html',
            controller: 'ViewdebugCtrl',
            reloadOnSearch: false
        })
        .when('/sdkeula', {
            templateUrl: Config.viewsDir + 'sdkeula.html',
            controller: 'SdkeulaCtrl'
        })
        .when('/sdkeula/:sdkFile', {
            templateUrl: Config.viewsDir + 'sdkeula.html',
            controller: 'SdkeulaCtrl'
        })
        .when('/setup', {
            templateUrl: Config.viewsDir + 'dialogs/setup.html',
            controller: 'SetupCtrl'
        })
        .when('/', {
            template: '',
            controller: 'MainCtrl'
        })
        .otherwise({
            redirectTo: '/'
        });
});
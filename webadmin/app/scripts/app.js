'use strict';

angular.module('webadminApp', [
    'ngCookies',
    'ngResource',
    'ngSanitize',
    'ngRoute',
    'ui.bootstrap'
])
    .config(function ($routeProvider) {
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
              templateUrl: 'views/login.html',
              controller: 'LoginCtrl'
            })
            .when('/advanced', {
                templateUrl: 'views/advanced.html',
                controller: 'AdvancedCtrl'
            })
            .when('/webclient', {
                templateUrl: 'views/webclient.html',
                controller: 'WebclientCtrl'
            })
            .when('/sdkeula', {
                templateUrl: 'views/sdkeula.html',
                controller: 'SdkeulaCtrl'
            })
            .otherwise({
                redirectTo: '/settings'
            });
    });

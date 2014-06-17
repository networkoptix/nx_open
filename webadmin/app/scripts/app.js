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
              templateUrl: 'views/login.html',
              controller: 'LoginCtrl'
            })
            .otherwise({
                redirectTo: '/settings'
            });
    });

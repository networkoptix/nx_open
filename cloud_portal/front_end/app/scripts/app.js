'use strict';

angular.module('cloudApp', [
    'ipCookie',
    'ngResource',
    'ngSanitize',
    'ngRoute',
    'ui.bootstrap',
    'ngStorage'
]).config(function ($routeProvider) {
    $routeProvider
        .when('/login', {
            templateUrl: 'views/login.html',
            controller: 'LoginCtrl'
        })
        .when('/register', {
            templateUrl: 'views/register.html',
            controller: 'LoginCtrl'
        })
        .when('/account/password', {
            templateUrl: 'views/account.html',
            controller: 'AccountCtrl',
            resolve: {
                test: function ($route) { $route.current.params.passwordMode = true; }
            }
        })
        .when('/account', {
            templateUrl: 'views/account.html',
            controller: 'AccountCtrl',
            resolve: {
                test: function ($route) { $route.current.params.accountMode = true; }
            }
        })
        .when('/systems', {
            templateUrl: 'views/systems.html',
            controller: 'SystemsCtrl'
        })

        .when('/activate/', {
            templateUrl: 'views/activate_restore.html',
            controller: 'ActivateRestoreCtrl',
            resolve: {
                test: function ($route) { $route.current.params.reactivating = true; }
            }
        })
        .when('/activate/:activateCode', {
            templateUrl: 'views/activate_restore.html',
            controller: 'ActivateRestoreCtrl'
        })
        .when('/restore_password/', {
            templateUrl: 'views/activate_restore.html',
            controller: 'ActivateRestoreCtrl',
            resolve: {
                test: function ($route) { $route.current.params.restoring = true; }
            }
        })
        .when('/restore_password/:restoreCode', {
            templateUrl: 'views/activate_restore.html',
            controller: 'ActivateRestoreCtrl'
        })

        .when('/debug', {
            templateUrl: 'views/debug.html',
            controller: 'DebugCtrl'
        })
        .otherwise({
            templateUrl: 'views/startPage.html',
            controller: 'StartPageCtrl'
        });
});




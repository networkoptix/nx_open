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
        .when('/register/success', {
            templateUrl: 'views/register_success.html',
            controller: 'LoginCtrl'
        })
        .when('/systems', {
            templateUrl: 'views/systems.html',
            controller: 'SystemsCtrl'
        })
        .when('/account', {
            templateUrl: 'views/account.html',
            controller: 'AccountCtrl'
        })
        .otherwise({
            templateUrl: 'views/startPage.html',
            controller: 'StartPageCtrl'
        });
});




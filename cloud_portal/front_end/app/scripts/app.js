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
        .otherwise({
            templateUrl: 'views/startPage.html',
            controller: 'StartPageCtrl'
        });
});




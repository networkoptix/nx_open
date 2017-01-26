'use strict';

angular.module('webadminApp', [
    'ngResource',
    'ngSanitize',
    'ngRoute',
    'ui.bootstrap',
    'ngStorage',
    'typeahead-focus'
]).config(function ($routeProvider) {
    $routeProvider
        .when('/setup', {
            templateUrl: 'views/dialogs/setup-inline.html',
            controller: 'SetupCtrl'
        })
        .otherwise({
            redirectTo: '/setup'
        });
});

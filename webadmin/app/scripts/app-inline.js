'use strict';

angular.module('webadminApp', [
    'nxCommon',
    'ngResource',
    'ngSanitize',
    'ngRoute',
    'ui.bootstrap',
    'ngStorage'
]).config(['$httpProvider', function ($httpProvider) {
    $httpProvider.defaults.xsrfCookieName = 'nx-vms-csrf-token';
    $httpProvider.defaults.xsrfHeaderName = 'Nx-Vms-Csrf-Token';
}]).config(['$routeProvider', function ($routeProvider) {
    $routeProvider
        .when('/setup', {
            templateUrl: Config.viewsDir + 'dialogs/setup-inline.html',
            controller: 'SetupCtrl'
        })
        .otherwise({
            redirectTo: '/setup'
        });
}]);

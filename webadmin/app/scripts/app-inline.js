'use strict';

angular.module('webadminApp', [
    'nxCommon',
    'ngResource',
    'ngSanitize',
    'ngRoute',
    'ui.bootstrap',
    'ngStorage',
    'typeahead-focus'
]).config(['$routeProvider', function ($routeProvider) {
    $routeProvider
        .when('/setup', {
            templateUrl: Config.viewsDir + 'dialogs/setup-inline.html',
            controller: 'SetupCtrl'
        })
        .otherwise({
            redirectTo: '/setup'
        });
}]);

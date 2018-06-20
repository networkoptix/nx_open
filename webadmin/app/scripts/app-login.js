'use strict';

angular.module('webadminApp', [
    'nxCommon',
    'ngResource',
    'ngSanitize',
    'ngRoute',
    'ui.bootstrap',
    'ngStorage'
]).config(['$httpProvider', function ($httpProvider) {
    $httpProvider.defaults.xsrfCookieName = 'x-runtime-guld';
    $httpProvider.defaults.xsrfHeaderName = 'X-Runtime-Guid';
}]).run(['mediaserver',function (mediaserver) {
    mediaserver.getCurrentUser();
}]);


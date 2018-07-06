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
}]).run(['mediaserver',function (mediaserver) {
    mediaserver.getCurrentUser();
}]);


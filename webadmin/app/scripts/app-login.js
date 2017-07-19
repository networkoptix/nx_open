'use strict';

angular.module('webadminApp', [
    'nxCommon',
    'ngResource',
    'ngSanitize',
    'ngRoute',
    'ui.bootstrap',
    'ngStorage'
]).run(['mediaserver',function (mediaserver) {
    mediaserver.getCurrentUser();
}]);


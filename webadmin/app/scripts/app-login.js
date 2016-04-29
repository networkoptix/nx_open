'use strict';

angular.module('webadminApp', [
    'ngResource',
    'ngSanitize',
    'ngRoute',
    'ui.bootstrap',
    'ui.select',
    'ngStorage'
]).run(['mediaserver',function (mediaserver) {
    mediaserver.getCurrentUser();
}]);


'use strict';

angular.module('webadminApp', [
    'ngResource',
    'ngSanitize',
    'ngRoute',
    'ui.bootstrap',
    'ngStorage'
]).run(['mediaserver',function (mediaserver) {
    mediaserver.getCurrentUser();
}]);


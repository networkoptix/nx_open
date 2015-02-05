'use strict';

angular.module('webadminApp')
    .controller('LogCtrl', function ($scope, mediaserver) {
        $scope.logUrl = mediaserver.logUrl();
    });
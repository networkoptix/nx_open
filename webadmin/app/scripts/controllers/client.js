'use strict';

angular.module('webadminApp').controller('ClientCtrl', function ($scope, nativeClient) {
    $scope.startCameras = function(){
        nativeClient.startCamerasMode();
    }
});

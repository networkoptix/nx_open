'use strict';

angular.module('webadminApp').controller('ClientCtrl', function ($scope, nativeClient, $log) {
    $scope.startCameras = function(){
        $log.log("Calling cameras mode");
        nativeClient.startCamerasMode();
    }
});

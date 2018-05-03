'use strict';

angular.module('webadminApp').controller('ClientCtrl', ['$scope', 'nativeClient', '$log', 'mediaserver',
function ($scope, nativeClient, $log, mediaserver) {
    $scope.Config = Config;
    $scope.port = window.location.port;
    $scope.IP = window.location.host;

    mediaserver.getModuleInformation().then(function (r) {
        $scope.settings = r.data.reply;
    });
    mediaserver.networkSettings().then(function(r){
        var settings = r.data.reply;
        $scope.IP = settings[0].ipAddr;
    });

    $scope.startCameras = function(){
        $log.log("Calling cameras mode");
        nativeClient.startCamerasMode();
    };
}]);

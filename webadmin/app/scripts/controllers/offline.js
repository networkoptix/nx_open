'use strict';

angular.module('webadminApp')
    .controller('OfflineCtrl', function ($scope, $modalInstance, $interval, mediaserver) {

        function reload(){
            window.location.reload();
            return false;
        }

        $scope.refresh = reload;
        function pingServer(){
            var request = mediaserver.getModuleInformation(true);
            request.then(function(){
                return reload();
            },function(){
                setTimeout(pingServer,1000);
                return false;
            });
        }

        $scope.state = L.offlineDialog.serverOffline;
        setTimeout(pingServer,1000);
    });
'use strict';

angular.module('webadminApp')
    .controller('OfflineCtrl', function ($scope, $modalInstance, $interval, mediaserver) {

        function reload(){
            window.location.reload();
            return false;
        }

        $scope.refresh = reload;
        function pingServer(){
            var request = mediaserver.getSettings(true);
            request.then(function(result){
                return reload();
            },function(){
                setTimeout(pingServer,1000);
                return false;
            });
        }

        $scope.state = 'server is offline';
        setTimeout(pingServer,1000);
    });
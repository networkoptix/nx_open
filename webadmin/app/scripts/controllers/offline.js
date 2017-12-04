'use strict';

angular.module('webadminApp')
    .run(['$http','$templateCache', function($http, $templateCache) {
        // Preload content into cache
        $http.get(Config.viewsDir + 'components/offline.html', {cache: $templateCache});
    }])
    .controller('OfflineCtrl', ['$scope', '$modalInstance', '$interval', 'mediaserver',
    function ($scope, $modalInstance, $interval, mediaserver) {

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
    }]);
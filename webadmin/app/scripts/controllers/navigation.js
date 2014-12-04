'use strict';

angular.module('webadminApp')
    .controller('NavigationCtrl', function ($scope, $location, mediaserver) {
        $scope.user = {
            isAdmin: true
        };

        mediaserver.getCurrentUser().then(function(result){
            $scope.user = {
                isAdmin: result.data.reply.isAdmin || (result.data.reply.permissions & Config.globalEditServersPermissions)
            };
        });

        mediaserver.getSettings().then(function (r) {
            $scope.settings = {
                name:r.data.reply.name,
                remoteAddresses:r.data.reply.remoteAddresses.join('\n')
            };
        });
        $scope.isActive = function (path) {
            var currentPath = $location.path().split('/')[1];
            return currentPath.split('?')[0] === path.split('/')[1].split('?')[0];
        };
    });

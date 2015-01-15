'use strict';

angular.module('webadminApp')
    .controller('NavigationCtrl', function ($scope, $location, mediaserver, $cookies) {
        $scope.user = {
            isAdmin: true
        };

        mediaserver.getCurrentUser().then(function(result){
            $scope.user = {
                isAdmin: result.data.reply.isAdmin || (result.data.reply.permissions & Config.globalEditServersPermissions)
            };
        });

        mediaserver.getSettings().then(function (r) {
            $scope.settings = r.data.reply;
            $scope.settings.remoteAddresses = $scope.settings.remoteAddresses.join('\n');
        });
        $scope.isActive = function (path) {
            var currentPath = $location.path().split('/')[1];
            return currentPath.split('?')[0] === path.split('/')[1].split('?')[0];
        };

        $scope.logout = function(){
            delete $cookies.realm;
            delete $cookies.nonce;
            delete $cookies.response;
            delete $cookies.username;
            window.location.reload();
        }
    });

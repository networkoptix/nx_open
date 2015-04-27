'use strict';

angular.module('webadminApp')
    .controller('NavigationCtrl', function ($scope, $location, mediaserver, ipCookie) {
        $scope.user = {
            isAdmin: true
        };

        mediaserver.checkAdmin().then(function(isAdmin){
            $scope.user = {
                isAdmin: isAdmin
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

            ipCookie.remove('response',{ path: '/' });
            ipCookie.remove('nonce',{ path: '/' });
            ipCookie.remove('realm',{ path: '/' });
            ipCookie.remove('username',{ path: '/' });
            window.location.reload();
        };
        $scope.alertVisible = true;
        $scope.closeAlert = function(){
            $scope.alertVisible = false;
        }
    });

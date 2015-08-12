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
            ipCookie.remove('auth', { path: '/' });
            ipCookie.remove('nonce',{ path: '/' });
            ipCookie.remove('realm',{ path: '/' });

            // TODO: REMOVE OBSOLETE COOKIES
            // ipCookie.remove('username',{ path: '/' });
            // ipCookie.remove('response',{ path: '/' });

            window.location.reload();
        };

        $scope.webclientEnabled = Config.webclientEnabled;
        $scope.alertVisible = !$scope.isActive("/view");

        $scope.closeAlert = function(){
            $scope.alertVisible = false;
        }
    });

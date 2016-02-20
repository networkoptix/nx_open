'use strict';

angular.module('webadminApp')
    .controller('NavigationCtrl', function ($scope, $location, mediaserver, ipCookie, $sessionStorage) {
        $scope.user = {
            isAdmin: true
        };

        mediaserver.getUser().then(function(user){
            $scope.user = {
                isAdmin: user.isAdmin,
                name: user.name
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
        $scope.alertVisible = true;

        $scope.session = $sessionStorage;

        $scope.closeAlert = function(){
            $scope.session.serverInfoAlertHidden = true;
        };
    });

'use strict';

angular.module('webadminApp')
    .controller('NavigationCtrl', function ($scope, $location, mediaserver, $sessionStorage) {
        $scope.user = {
            isAdmin: true
        };

        mediaserver.getModuleInformation().then(function (r) {
            $scope.settings = r.data.reply;
            $scope.settings.remoteAddresses = $scope.settings.remoteAddresses.join('\n');

            // check for safe mode and new server and redirect.
            if(r.data.reply.serverFlags.includes(Config.newServerFlag) && !r.data.reply.ecDbReadOnly){
                $location.path("/setup");
                return;
            }
            mediaserver.getUser().then(function(user){
                $scope.user = {
                    isAdmin: user.isAdmin,
                    name: user.name
                };
            });

        });
        $scope.isActive = function (path) {
            var local_path = $location.path();
            if(!local_path ){
                return false;
            }
            var currentPath = local_path.split('/')[1];
            return currentPath.split('?')[0] === path.split('/')[1].split('?')[0];
        };

        $scope.logout = function(){
            mediaserver.logout().then(function(){
                window.location.reload();
            });
        };

        $scope.webclientEnabled = Config.webclientEnabled;
        $scope.alertVisible = true;

        $scope.session = $sessionStorage;

        $scope.closeAlert = function(){
            $scope.session.serverInfoAlertHidden = true;
        };
    });

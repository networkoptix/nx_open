'use strict';

angular.module('webadminApp')
    .controller('NavigationCtrl', function ($scope, $location, mediaserver, dialogs) {
        $scope.user = {
            isAdmin: true
        };

        mediaserver.getModuleInformation().then(function (r) {
            $scope.settings = r.data.reply;

            mediaserver.resolveNewSystemAndUser().then(function(user){
                if(user === null){
                    return;
                }
                $scope.user = {
                    isAdmin: user.isAdmin,
                    name: user.name
                };
            },function(error){
                if(error.status !== 401 && error.status !== 403) {
                    dialogs.alert(L.navigaion.cannotGetUser);
                }
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
    });

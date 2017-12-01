'use strict';

angular.module('webadminApp')
    .controller('NavigationCtrl', ['$scope', '$location', 'mediaserver', 'dialogs', 'nativeClient',
    function ($scope, $location, mediaserver, dialogs, nativeClient) {
        $scope.user = {
            isAdmin: true
        };
        $scope.noPanel = true;

        nativeClient.init().then(function(result){
            $scope.settings.liteClient = result.lite;
            $('body').addClass('lite-client-mode');
        });

        $scope.hasProxy = mediaserver.hasProxy();


        mediaserver.getModuleInformation().then(function (r) {
            $scope.settings = r.data.reply;
            $scope.noPanel = $scope.settings.flags.noHDD || $scope.settings.flags.cleanSystem;
            if(!$scope.noPanel) {
                mediaserver.resolveNewSystemAndUser().then(function (user) {
                    if (user === null) {
                        return;
                    }
                    $scope.user = user;
                }, function (error) {
                    if (error.status !== 401 && error.status !== 403) {
                        dialogs.alert(L.navigaion.cannotGetUser);
                    }
                });
            }
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
    }]);

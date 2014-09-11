'use strict';

angular.module('webadminApp')
    .controller('SettingsCtrl', function ($scope, $modal, $log, mediaserver) {
        $scope.settings = mediaserver.getSettings();

        $scope.settings.then(function (r) {
            $scope.settings = {
                systemName: r.data.reply.systemName,
                port: r.data.reply.port
            };
        });

        $scope.password = '';
        $scope.oldPassword = '';
        $scope.confirmPassword = '';

        $scope.openJoinDialog = function () {
            var modalInstance = $modal.open({
                templateUrl: 'views/join.html',
                controller: 'JoinCtrl',
                resolve: {
                    items: function () {
                        return $scope.settings;
                    }
                }
            });

            modalInstance.result.then(function (settings) {
                $log.info(settings);
                mediaserver.joinSystem(settings.url,settings.password).then(function(r){
                    if(r.data.error!=0){
                        var errorToShow = r.data.errorString;
                        switch(errorToShow){
                            case 'FAIL':
                                errorToShow = "System is unreachable or doesn't exist.";
                                break;
                            case 'UNAUTHORIZED':
                                errorToShow = "Wrong password.";
                                break;
                            case 'INCOMPATIBLE':
                                errorToShow = "Found system has incompatible version.";
                                break;
                        }
                        alert("Connection failed. " + errorToShow);
                    }else {
                        alert("Connection succeed.");
                    }
                });
            });
        };

        function restartServer(){
            mediaserver.restart();
        }

        function successHandler (r){
            console.log(r);
            if(r.data.reply.restartNeeded){
                if(confirm("Changes will be applied after restart. Do you want to restart server now?")){
                    restartServer();
                }
            }
        }

        $scope.save = function () {
            mediaserver.saveSettings($scope.settings.systemName,$scope.settings.port).then(successHandler);
        };

        $scope.changePassword = function () {
            if($scope.password == $scope.confirmPassword)
                mediaserver.changePassword($scope.password,$scope.oldPassword).then(successHandler);
        };

        $scope.restart = function () {
            if(confirm("Do you want to restart server now?")){
                restartServer();
            }
        };
    });

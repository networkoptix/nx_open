'use strict';

angular.module('webadminApp')
    .controller('SettingsCtrl', function ($scope, $modal, $log, mediaserver,$location) {

        mediaserver.getCurrentUser().success(function(result){
            if(!result.reply.isAdmin){
                $location.path("/info"); //no admin rights - redirect
                return;
            }
        });

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
                mediaserver.mergeSystems(settings.url,settings.password,settings.keepMySystem).then(function(r){
                    if(r.data.error!=0){
                        var errorToShow = r.data.errorString;
                        switch(errorToShow){
                            case 'FAIL':
                                errorToShow = "System is unreachable or doesn't exist.";
                                break;
                            case 'UNAUTHORIZED':
                            case 'password':
                                errorToShow = "Wrong password.";
                                break;
                            case 'INCOMPATIBLE':
                                errorToShow = "Found system has incompatible version.";
                                break;
                            case 'url':
                                errorToShow = "Wrong url.";
                                break;
                        }
                        alert("Merge failed: " + errorToShow);
                    }else {
                        alert("Merge succeed.");
                        window.location.reload();
                    }
                });
            });
        };

        function restartServer(){
            $modal.open({
                templateUrl: 'views/restart.html',
                controller: 'RestartCtrl',
                resolve:{
                    port:function(){
                        return $scope.settings.port;
                    }
                }
            });
        }

        function errorHandler(r){
            alert ("Connection error")
            return false;
        }
        function resultHandler (r){
            if(r.error!=0) {
                var errorToShow = r.errorString;
                switch (errorToShow) {
                    case 'UNAUTHORIZED':
                    case 'password':
                        errorToShow = "Wrong password.";
                }
                alert("Error: " + errorToShow);
            }else if (r.reply.restartNeeded) {
                if (confirm("All changes saved. New settings will be applied after restart. \n Do you want to restart server now?")) {
                    restartServer();
                }
            } else {
                alert("Settings saved");
            }
        }

        $scope.save = function () {
            mediaserver.saveSettings($scope.settings.systemName,$scope.settings.port).success(resultHandler).error(errorHandler);
        };

        $scope.changePassword = function () {
            if($scope.password == $scope.confirmPassword)
                mediaserver.changePassword($scope.password,$scope.oldPassword).success(resultHandler).error(errorHandler);
        };

        $scope.restart = function () {
            if(confirm("Do you want to restart server now?")){
                restartServer();
            }
        };
    });

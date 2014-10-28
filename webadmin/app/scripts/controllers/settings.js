'use strict';

angular.module('webadminApp')
    .controller('SettingsCtrl', function ($scope, $modal, $log, mediaserver,$location,$timeout) {

        mediaserver.getCurrentUser().success(function(result){
            if(!result.reply.isAdmin){
                $location.path("/info"); //no admin rights - redirect
                return;
            }
        });


        mediaserver.getSettings().then(function (r) {
            $scope.settings = {
                systemName: r.data.reply.systemName,
                port: r.data.reply.port,
                id: r.data.reply.id
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

        function restartServer(passPort){
            $modal.open({
                templateUrl: 'views/restart.html',
                controller: 'RestartCtrl',
                resolve:{
                    port:function(){
                        return passPort?$scope.settings.port:null;
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
                    restartServer(true);
                }
            } else {
                alert("Settings saved");
                if( $scope.settings.port !=  window.location.port )
                    window.location.href =  window.location.protocol + "//" + window.location.hostname + ":" + $scope.settings.port;
            }
        }

        $scope.save = function () {

            if($scope.settingsForm.$valid) {
                mediaserver.saveSettings($scope.settings.systemName, $scope.settings.port).success(resultHandler).error(errorHandler);
            }else{
               alert("form is not valid");
            }
        };

        $scope.changePassword = function () {
            if($scope.password == $scope.confirmPassword)
                mediaserver.changePassword($scope.password,$scope.oldPassword).success(resultHandler).error(errorHandler);
        };

        $scope.restart = function () {
            if(confirm("Do you want to restart server now?")){
                restartServer(false);
            }
        };

        function checkServersIp(server,i){
            var ips = server.networkAddresses.split(';');
            var port = server.apiUrl.substring(server.apiUrl.lastIndexOf(":"));
            var url = "http://" + ips[i] + port;

            mediaserver.getSettings(url).then(function(){
                server.apiUrl = url;
            }).catch(function(){
                if(i < ips.length-1)
                    checkServersIp (server,i+1);
                else
                    server.status = "Unavailable";
                return false;
            });
        }

        mediaserver.getMediaServers().success(function(data){
            $scope.mediaServers = _.sortBy(data,function(server){
                return (server.status=='Online'?'0':'1') + server.Name + server.id;
                // Сортировка: online->name->id
            });
            $timeout(function() {
                _.each($scope.mediaServers, function (server) {
                    var i = 0;//1. Опрашиваем айпишники подряд
                    checkServersIp(server, i);
                });
            },1000);
        });
    });

'use strict';

angular.module('webadminApp')
    .controller('SettingsCtrl', function ($scope, $modal, $log, mediaserver,$location,$timeout) {

        mediaserver.checkAdmin().then(function(isAdmin){
            if(!isAdmin){
                $location.path('/info'); //no admin rights - redirect
                return;
            }
        });

        mediaserver.getSettings().then(function (r) {
            $scope.settings = {
                systemName: r.data.reply.systemName,
                port: r.data.reply.port,
                id: r.data.reply.id
            };

            $scope.oldSystemName = r.data.reply.systemName;
            $scope.oldPort = r.data.reply.port;
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
                mediaserver.mergeSystems(settings.url,settings.password,settings.currentPassword,settings.keepMySystem).then(function(r){
                    if(r.data.error!=='0'){
                        var errorToShow = r.data.errorString;
                        switch(errorToShow){
                            case 'FAIL':
                                errorToShow = 'System is unreachable or doesn\'t exist.';
                                break;
                            case 'UNAUTHORIZED':
                            case 'password':
                            case 'PASSWORD':
                                errorToShow = 'Wrong password.';
                                break;
                            case 'INCOMPATIBLE':
                                errorToShow = 'Found system has incompatible version.';
                                break;
                            case 'url':
                            case 'URL':
                                errorToShow = 'Wrong url.';
                                break;
                        }
                        alert('Merge failed: ' + errorToShow);
                    }else {
                        alert('Merge succeed.');
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

        function errorHandler(){
            alert ('Connection error');
            return false;
        }
        function resultHandler (r){
            var data = r.data;

            if(data.error!=='0') {
                //console.log('some error',data);
                var errorToShow = data.errorString;
                switch (errorToShow) {
                    case 'UNAUTHORIZED':
                    case 'password':
                    case 'PASSWORD':
                        errorToShow = 'Wrong password.';
                }
                alert('Error: ' + errorToShow);
            }else if (data.reply.restartNeeded) {
                if (confirm('All changes saved. New settings will be applied after restart. \n Do you want to restart server now?')) {
                    restartServer(true);
                }
            } else {
                alert('Settings saved');
                if( $scope.settings.port !==  window.location.port ) {
                    window.location.href = window.location.protocol + '//' + window.location.hostname + ':' + $scope.settings.port;
                }else{
                    window.location.reload();
                }
            }
        }

        $scope.save = function () {


            if($scope.settingsForm.$valid) {
                if($scope.oldSystemName !== $scope.settings.systemName &&
                    !confirm('If there are others servers in local network with "' + $scope.settings.systemName +
                        '" system name then it could lead to this server settings loss. Continue?')){
                    $scope.settings.systemName = $scope.oldSystemName;
                }

                if($scope.oldSystemName !== $scope.settings.systemName  || $scope.oldPort !== $scope.settings.port ) {
                    mediaserver.saveSettings($scope.settings.systemName, $scope.settings.port).then(resultHandler, errorHandler);
                }
            }else{
                alert('form is not valid');
            }
        };

        $scope.changePassword = function () {
            if($scope.password === $scope.confirmPassword) {
                mediaserver.changePassword($scope.password, $scope.oldPassword).then(resultHandler, errorHandler);
            }
        };

        $scope.restart = function () {
            if(confirm('Do you want to restart server now?')){
                restartServer(false);
            }
        };

        function checkServersIp(server,i){
            var ips = server.networkAddresses.split(';');
            var port = server.apiUrl.substring(server.apiUrl.lastIndexOf(':'));
            var url = 'http://' + ips[i] + port;

            mediaserver.getSettings(url).then(function(){
                server.apiUrl = url;
            },function(){
                if(i < ips.length-1) {
                    checkServersIp(server, i + 1);
                }
                else {
                    server.status = 'Unavailable';

                    $scope.mediaServers = _.sortBy($scope.mediaServers,function(server){
                        return (server.status==='Online'?'0':'1') + server.Name + server.id;
                        // Сортировка: online->name->id
                    });

                }
                return false;
            });
        }

        mediaserver.getMediaServers().then(function(data){
            $scope.mediaServers = _.sortBy(data.data,function(server){
                return (server.status==='Online'?'0':'1') + server.Name + server.id;
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

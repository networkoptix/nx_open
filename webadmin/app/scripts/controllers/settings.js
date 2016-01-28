'use strict';

angular.module('webadminApp')
    .controller('SettingsCtrl', function ($scope, $modal, $log, mediaserver,$location,$timeout) {

        mediaserver.getUser().then(function(user){
            if(!user.isAdmin){
                $location.path('/info'); //no admin rights - redirect
                return;
            }

            $scope.canMerge = user.isOwner;
        });

        mediaserver.getSettings().then(function (r) {
            $scope.settings = {
                systemName: r.data.reply.systemName,
                port: r.data.reply.port,
                id: r.data.reply.id,
                ecDbReadOnly:r.data.reply.ecDbReadOnly
            };

            $scope.oldSystemName = r.data.reply.systemName;
            $scope.oldPort = r.data.reply.port;
        });

        $scope.password = '';
        $scope.oldPassword = '';
        $scope.confirmPassword = '';

        $scope.openJoinDialog = function () {
            $modal.open({
                templateUrl: 'views/join.html',
                controller: 'JoinCtrl',
                resolve: {
                    items: function () {
                        return $scope.settings;
                    }
                }
            });
        };


        $scope.openDisconnectDialog = function () {
            $modal.open({
                templateUrl: 'views/disconnectServer.html',
                controller: 'DisconnectServerCtrl',
                resolve: {
                    system:function(){
                        return null;
                    }
                }
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
                if( $scope.settings.port !=  window.location.port ) {
                    window.location.href = (window.location.protocol + '//' + window.location.hostname + ':' + $scope.settings.port + window.location.pathname + window.location.hash);
                }else{
                    window.location.reload();
                }
            }
        }

        $scope.save = function () {
            if($scope.settingsForm.$valid) {
                mediaserver.changePort($scope.settings.port).then(resultHandler, errorHandler);
            }else{
                alert('form is not valid');
            }
        };

// execute/scryptname&mode
        $scope.restart = function () {
            if(confirm('Do you want to restart server now?')){
                restartServer(false);
            }
        };
        $scope.canHardwareRestart = false;
        $scope.canRestoreSettings = false;
        $scope.canRestoreSettingsNotNetwork = false;

        mediaserver.getScripts().then(function(data){
            if(data.data && data.data.reply) {
                $scope.canHardwareRestart = data.data.reply.indexOf('reboot') >= 0;
                $scope.canRestoreSettings = data.data.reply.indexOf('restore') >= 0;
                $scope.canRestoreSettingsNotNetwork = data.data.reply.indexOf('restore_keep_ip') >= 0;
            }
        });

        $scope.hardwareRestart = function(){
            if(confirm('Do you want to restart server\'s operation system?')){
                mediaserver.execute('reboot').then(resultHandler, errorHandler);
            }
        };

        $scope.restoreSettings = function(){
            if(confirm('Do you want to restart all server\'s settings? Archive will be saved, but network settings will be reset.')){
                mediaserver.execute('restore').then(resultHandler, errorHandler);
            }
        };

        $scope.restoreSettingsNotNetwork = function(){
            if(confirm('Do you want to restart all server\'s settings? Archive and network settings will be saved.')){
                mediaserver.execute('restore_keep_ip').then(resultHandler, errorHandler);
            }
        };

        function checkServersIp(server,i){
            var ips = server.networkAddresses.split(';');
            var port = server.apiUrl.substring(server.apiUrl.lastIndexOf(':'));
            var url = window.location.protocol + '//' + ips[i] + port;

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

'use strict';

angular.module('webadminApp')
    .controller('SettingsCtrl', function ($scope, $modal, $log, mediaserver,cloudAPI,$location,$timeout, dialogs) {


        mediaserver.getUser().then(function(user){
            if(!user.isAdmin){
                $location.path('/info'); //no admin rights - redirect
                return;
            }

            $scope.canMerge = user.isOwner;
        });

        mediaserver.getModuleInformation().then(function (r) {
            $scope.settings = {
                systemName: r.data.reply.systemName,
                port: r.data.reply.port,
                id: r.data.reply.id,
                ecDbReadOnly:r.data.reply.ecDbReadOnly
            };

            $scope.oldSystemName = r.data.reply.systemName;
            $scope.oldPort = r.data.reply.port;
        });

        $scope.Config = Config;
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


        $scope.openRestoreDefaultsDialog = function () {
            //1. confirm detach
            var confirmation = 'Do you want to clear database and settings?';
            dialogs.confirmWithPassword(null, confirmation, 'Restore factory defaults').then(function(oldPassword){
                mediaserver.restoreFactoryDefaults(oldPassword).then(function(data){
                    if(data.data.error !== '0' && data.data.error !== 0){
                        // Some Error has happened
                        dialogs.alert(data.data.errorString);
                        return;
                    }
                    //2. reload page - he will be redirected to master
                    window.location.reload();
                },function(error){
                    dialogs.alert('Can\'t proceed with action: unexpected error has happened');
                    $log.log("can't detach");
                    $log.error(error);
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
            dialogs.alert ('Connection error');
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
                dialogs.alert('Error: ' + errorToShow);
            }else if (data.reply.restartNeeded) {
                dialogs.confirm('All changes saved. New settings will be applied after restart. \n Do you want to restart server now?').then(function() {
                    restartServer(true);
                });
            } else {
                dialogs.alert('Settings saved');
                if( $scope.settings.port !==  window.location.port ) {
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
                dialogs.alert('form is not valid');
            }
        };

// execute/scryptname&mode
        $scope.restart = function () {
            dialogs.confirm('Do you want to restart server now?').then(function(){
                restartServer(false);
            });
        };
        $scope.canHardwareRestart = false;
        $scope.canRestoreSettings = false;
        $scope.canRestoreSettingsNotNetwork = false;

        mediaserver.getScripts().then(function(data){
            if(data.data && data.data.reply) {
                $scope.canHardwareRestart = data.data.reply.indexOf('reboot') >= 0;
                $scope.canRestoreSettings = data.data.reply.indexOf('restore') >= 0;
                $scope.canRestoreSettingsNotNetwork = data.data.reply.indexOf('restore_keep_ip') >= 0;
                $scope.canRunClient = data.data.reply.indexOf('start_lite_client') >= 0;
                $scope.canStopClient = data.data.reply.indexOf('stop_lite_client') >= 0;
            }
        });

        $scope.runClient = function(){
            mediaserver.execute('start_lite_client').then(resultHandler, errorHandler);
        };

        $scope.stopClient = function(){
            mediaserver.execute('stop_lite_client').then(resultHandler, errorHandler);
        };

        $scope.renameSystem = function(){
            mediaserver.changeSystemName($scope.settings.systemName).then(resultHandler, errorHandler);
        };

        $scope.hardwareRestart = function(){
            dialogs.confirm('Do you want to restart server\'s operation system?').then(function(){
                mediaserver.execute('reboot').then(resultHandler, errorHandler);
            });
        };

        $scope.restoreSettings = function(){
            dialogs.confirm('Do you want to restart all server\'s settings? Archive will be saved, but network settings will be reset.').then(function(){
                mediaserver.execute('restore').then(resultHandler, errorHandler);
            });
        };

        $scope.restoreSettingsNotNetwork = function(){
            dialogs.confirm('Do you want to restart all server\'s settings? Archive and network settings will be saved.').then(function(){
                mediaserver.execute('restore_keep_ip').then(resultHandler, errorHandler);
            });
        };

        function checkServersIp(server,i){
            var ips = server.networkAddresses.split(';');

            var port = '';
            if(ips[i].indexOf(':') < 0){
                port = server.apiUrl.substring(server.apiUrl.lastIndexOf(':'));
            }

            var url = window.location.protocol + '//' + ips[i] + port;

            mediaserver.getModuleInformation(url).then(function(){
                server.apiUrl = url;
            },function(error){
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
                // Set active state for server
                server.active = $scope.settings.id.replace('{','').replace('}','') === server.id.replace('{','').replace('}','');
                return (server.status==='Online'?'0':'1') + server.Name + server.id;
                // Sorting: online->name->id
            });
            $timeout(function() {
                _.each($scope.mediaServers, function (server) {
                    var i = 0;//1. Опрашиваем айпишники подряд
                    checkServersIp(server, i);
                });
            },1000);
        });

        mediaserver.systemCloudInfo().then(function(data){
            $scope.cloudSystemID = data.cloudSystemID;
            $scope.cloudAccountName = data.cloudAccountName;
        },function(){
            $scope.cloudSystemID = null;
            $scope.cloudAccountName = null;

            mediaserver.checkInternet().then(function (hasInternetOnServer) {
                $scope.hasInternetOnServer = hasInternetOnServer;
            });

            cloudAPI.checkConnection().then(function () {
                $scope.hasInternetOnClient = true;
            }, function (error) {
                $scope.hasInternetOnClient = false;
                if (error.data && error.data.resultCode) {
                    $scope.settings.internetError = formatError(error.data.resultCode);
                } else {
                    $scope.settings.internetError = "Couldn't check cloud connection";
                }
            });
        });


        function openCloudDialog(connect){
            $modal.open({
                templateUrl: 'views/dialogs/cloudDialog.html',
                controller: 'CloudDialogCtrl',
                backdrop:'static',
                size:'sm',
                keyboard:false,
                resolve:{
                    connect:function(){
                        return connect;
                    },
                    systemName:function(){
                        return $scope.settings.systemName;
                    },
                    cloudAccountName:function(){
                        return $scope.cloudAccountName;
                    },
                    cloudSystemID:function(){
                        return $scope.cloudSystemID;
                    }
                }
            }).result.finally(function(){
                window.location.reload();
            });
        }
        $scope.disconnectFromCloud = function() { // Disconnect from Cloud
            //Open Disconnect Dialog
            openCloudDialog(false);
        };

        $scope.connectToCloud = function() { // Connect to Cloud
            //Open Connect Dialog
            openCloudDialog(true);
        };



    });

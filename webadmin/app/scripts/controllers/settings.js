'use strict';

angular.module('webadminApp')
    .controller('SettingsCtrl', function ($scope,$rootScope, $modal, $log, mediaserver,cloudAPI,$location,$timeout, dialogs) {


        function updateActive(){
            $scope.active={
                system: $location.path() === '/settings/system',
                server: $location.path() === '/settings/server'
            };
        }
        updateActive();

        var killSubscription = $rootScope.$on('$routeChangeStart', updateActive);
        $scope.$on( '$destroy', function( ) {
            killSubscription();
        });

        mediaserver.getModuleInformation().then(function (r) {
            var data = r.data.reply;

            if(data.flags.newSystem){
                return;
            }

            $scope.settings = data;
            // $scope.noNetwork =

            $scope.oldSystemName = data.systemName;
            $scope.oldPort = data.port;
            checkUserRights();
        });

        function checkUserRights() {
            return mediaserver.getUser().then(function (user) {
                if (!user.isAdmin) {
                    $location.path('/info'); //no admin rights - redirect
                    return false;
                }

                $scope.$watch("active.system",function(){
                    if( $scope.active.system){
                        $location.path('/settings/system');
                    }
                });
                $scope.$watch("active.server",function(){
                    if( $scope.active.server){
                        $location.path('/settings/server');
                    }
                });

                $scope.canConnectToCloud = $scope.canMerge = user.isOwner;

                getCloudInfo();
                requestScripts();

                $scope.user = user;
            });
        }
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
            dialogs.confirmWithPassword(null, L.settings.confirmRestoreDefault, L.settings.confirmRestoreDefaultTitle).then(function(oldPassword){

                mediaserver.checkCurrentPassword(oldPassword).then(function() {
                    mediaserver.restoreFactoryDefaults().then(function (data) {
                        if (data.data.error !== '0' && data.data.error !== 0) {
                            // Some Error has happened
                            dialogs.alert(data.data.errorString);
                            return;
                        }

                        restartServer();
                    }, function (error) {
                        dialogs.alert(L.settings.unexpectedError);
                        $log.log('can\'t detach');
                        $log.error(error);
                    });
                },function(){
                    dialogs.alert(L.settings.wrongPassword);
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
            dialogs.alert (L.settings.connnetionError);
            return false;
        }
        function resultHandler (r, message){
            var data = r.data;

            if(data.error!=='0') {
                var errorToShow = data.errorString;
                switch (errorToShow) {
                    case 'UNAUTHORIZED':
                    case 'password':
                    case 'PASSWORD':
                        errorToShow = L.settings.wrongPassword;
                }
                dialogs.alert( L.settings.error + errorToShow);
            }else if (data.reply.restartNeeded) {
                dialogs.confirm(L.settings.restartNeeded).then(function() {
                    restartServer(true);
                });
            } else {
                dialogs.alert(message || L.settings.settingsSaved);
                if(!message) {
                    if ($scope.settings.port !== window.location.port) {
                        window.location.href = (window.location.protocol + '//' + window.location.hostname + ':' + $scope.settings.port + window.location.pathname + window.location.hash);
                    } else {
                        window.location.reload();
                    }
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
            dialogs.confirm(L.settings.confirmRestart).then(function(){
                restartServer(false);
            });
        };
        $scope.canHardwareRestart = false;
        $scope.canRestoreSettings = false;
        $scope.canRestoreSettingsNotNetwork = false;

        function requestScripts() {
            return mediaserver.getScripts().then(function (data) {
                if (data.data && data.data.reply) {
                    $scope.canHardwareRestart = data.data.reply.indexOf('reboot') >= 0;
                    $scope.canRestoreSettings = data.data.reply.indexOf('restore') >= 0;
                    $scope.canRestoreSettingsNotNetwork = data.data.reply.indexOf('restore_keep_ip') >= 0;
                    $scope.canRunClient = data.data.reply.indexOf('start_lite_client') >= 0;
                    $scope.canStopClient = data.data.reply.indexOf('stop_lite_client') >= 0;
                }
            });
        }


        function runResultHandler(result){
            resultHandler(result, L.settings.nx1ControlHint);
        }
        $scope.runClient = function(){
            mediaserver.execute('start_lite_client').then(runResultHandler, errorHandler);
        };

        $scope.stopClient = function(){
            mediaserver.execute('stop_lite_client').then(resultHandler, errorHandler);
        };

        $scope.renameSystem = function(){
            mediaserver.changeSystemName($scope.settings.systemName).then(resultHandler, errorHandler);
        };

        $scope.hardwareRestart = function(){
            dialogs.confirm(L.settings.confirmHardwareRestart).then(function(){
                mediaserver.execute('reboot').then(resultHandler, errorHandler);
            });
        };

        $scope.restoreSettings = function(){
            dialogs.confirm(L.settings.confirmRestoreSettings).then(function(){
                mediaserver.execute('restore').then(resultHandler, errorHandler);
            });
        };

        $scope.restoreSettingsNotNetwork = function(){
            dialogs.confirm(L.settings.confirmRestoreSettingsNotNetwork).then(function(){
                mediaserver.execute('restore_keep_ip').then(resultHandler, errorHandler);
            });
        };


        function getCloudInfo() {
            return mediaserver.systemCloudInfo().then(function (data) {
                $scope.cloudSystemID = data.cloudSystemID;
                $scope.cloudAccountName = data.cloudAccountName;
            }, function () {
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
                        $scope.settings.internetError = L.settings.couldntCheckInternet;
                    }
                });
            });
        }

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

        $scope.changePassword = function(){
            dialogs.confirmWithPassword(
                L.settings.confirmChangePasswordTitle,
                L.settings.confirmChangePassword,
                L.settings.confirmChangePasswordAction,
                'danger').then(function (oldPassword) {
                    //1. Check password
                    return mediaserver.checkCurrentPassword(oldPassword).then(function() {
                        // 1. Check for another enabled owner. If there is one - request login and password for him - open dialog
                        mediaserver.changeAdminPassword($scope.settings.rootPassword).then(resultHandler, errorHandler);
                    },function(){
                        dialogs.alert(L.settings.wrongPassword);
                    });
                });
        };
        $scope.disconnectFromCloud = function() { // Disconnect from Cloud
            //Open Disconnect Dialog

            function doDisconnect(localLogin,localPassword){
                // 2. Send request to the system only
                return mediaserver.disconnectFromCloud(localLogin, localPassword).then(function(){
                    window.location.reload();
                }, function(error){
                    console.error(error);
                    dialogs.alert(L.settings.unexpectedError);
                });

                /*

                 // Old code: request to cloud and to the system

                 cloudAPI.disconnect(cloudSystemID, $scope.settings.cloudEmail, $scope.settings.cloudPassword).then(
                 function () {
                 //2. Save settings to local server
                 mediaserver.clearCloudSystemCredentials().then(successHandler, errorHandler);
                 }, cloudErrorHandler);
                 */
            }
            dialogs.confirmWithPassword(
                L.settings.confirmDisconnectFromCloudTitle,
                L.settings.confirmDisconnectFromCloud,
                L.settings.confirmDisconnectFromCloudAction,
                'danger').then(function (oldPassword) {
                //1. Check password
                return mediaserver.checkCurrentPassword(oldPassword).then(function() {
                    // 1. Check for another enabled owner. If there is one - request login and password for him - open dialog
                    mediaserver.getUsers().then(function(result){
                        var localAdmin = _.find(result.data,function(user){
                            return user.isAdmin && user.isEnabled && !user.isCloud;
                        });
                        if(!localAdmin){
                            // Request for login and password
                            dialogs.createUser(L.settings.createLocalOwner, L.settings.createLocalOwnerTitle).then(function(data){
                                doDisconnect(data.login,data.password);
                            })
                        }else{
                            doDisconnect();
                        }
                    });
                },function(){
                    dialogs.alert(L.settings.wrongPassword);
                });
            });
        };

        $scope.connectToCloud = function() { // Connect to Cloud
            openCloudDialog(true); //Open Connect Dialog
        };



    });

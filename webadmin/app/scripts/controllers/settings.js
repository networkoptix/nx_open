'use strict';

angular.module('webadminApp')
    .controller('SettingsCtrl', ['$scope', '$rootScope', '$modal', '$log', 'mediaserver', '$poll', '$localStorage',
                                 'cloudAPI', '$location', '$timeout', 'dialogs', 'nativeClient',
    function ($scope, $rootScope, $modal, $log, mediaserver, $poll, $localStorage, cloudAPI, $location,
              $timeout, dialogs, nativeClient) {

        if(mediaserver.hasProxy()){
            $location.path("/view");
            return;
        }
        $scope.L = L;

        function updateActive(){
            $scope.active={
                device: $location.path() === '/settings/device',
                system: $location.path() === '/settings/system',
                server: $location.path() === '/settings/server'
            };
        }
        updateActive();

        var killSubscription = $rootScope.$on('$routeChangeStart', updateActive);
        $scope.$on( '$destroy', function( ) {
            killSubscription();
        });

        function pingModule(){
            return mediaserver.getModuleInformation(true).then(function(r){
                if(!data.flags.brokenSystem){
                     window.location.reload();
                }
                return true;
            });
        }

        nativeClient.init().then(function(result){
            $scope.mode={liteClient: result.lite};
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

            if(data.flags.brokenSystem){
                $poll(pingModule,1000);
                return;
            }
            
            if(data.flags.canSetupNetwork){
                mediaserver.networkSettings().then(function(r){
                    $scope.networkSettings = r.data.reply;
                });
            }
            checkUserRights();
        });

        function checkUserRights() {
            return mediaserver.getUser().then(function (user) {
                if (!user.isAdmin) {
                    $location.path('/info'); //no admin rights - redirect
                    return false;
                }

                $scope.$watch("active.device",function(){
                    if( $scope.active.device){
                        $location.path('/settings/device',false);
                    }
                });
                $scope.$watch("active.system",function(){
                    if( $scope.active.system){
                        $location.path('/settings/system',false);
                    }
                });
                $scope.$watch("active.server",function(){
                    if( $scope.active.server){
                        $location.path('/settings/server',false);
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
                templateUrl: Config.viewsDir + 'join.html',
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
                templateUrl: Config.viewsDir + 'restart.html',
                controller: 'RestartCtrl',
                resolve:{
                    port:function(){
                        return passPort?$scope.settings.port:null;
                    }
                }
            });
        }

        function errorHandler(result){
            if(result == 'cancel'){ // That's fine, dialog was cancelled
                return false;
            }
            dialogs.alert (L.settings.connnetionError);
            return false;
        }
        var changedPort = false;
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
            }else if (data.reply && data.reply.restartNeeded) {
                dialogs.confirm(L.settings.restartNeeded).then(function() {
                    restartServer(true);
                });
            } else {
                dialogs.alert(message || L.settings.settingsSaved).finally(function(){
                    if(!message) {
                        if (changedPort) {
                            window.location.href =
                                window.location.protocol + '//' +
                                window.location.hostname + ':' +
                                $scope.settings.port +
                                window.location.pathname +
                                window.location.hash + '?' +
                                nativeClient.webSocketUrlPath();
                        } else {
                            window.location.reload();
                        }
                    }
                });
            }
        }

        $scope.save = function () {
            if($scope.settings.port <= 1024){
                dialogs.confirm(L.settings.unsafePortConfirm).then(function(){
                    changedPort = true;
                    mediaserver.changePort($scope.settings.port).then(resultHandler, errorHandler);
                });
            }else {
                changedPort = true;
                mediaserver.changePort($scope.settings.port).then(resultHandler, errorHandler);
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

                    // Has any scripts
                    $scope.controlDevice = data.data.reply && data.data.reply.length>0;

                    $scope.canHardwareRestart = data.data.reply.indexOf('reboot') >= 0;
                    $scope.canRestoreSettings = data.data.reply.indexOf('restore') >= 0;
                    $scope.canRestoreSettingsNotNetwork = data.data.reply.indexOf('restore_keep_ip') >= 0;
                }
            });
        }


        function runResultHandler(result){
            resultHandler(result, L.settings.nx1ControlHint);
        }

        $scope.renameSystem = function(){
            mediaserver.changeSystemName($scope.settings.systemName).then(resultHandler, errorHandler);
        };

        $scope.hardwareRestart = function(settingsSaved){
            dialogs.confirm(settingsSaved?L.settings.restartNeeded:L.settings.confirmHardwareRestart).then(function(){
                mediaserver.execute('reboot').then(restartServer, errorHandler);
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

        function openCloudDialog(){
            $modal.open({
                templateUrl: Config.viewsDir + 'dialogs/cloudDialog.html',
                controller: 'CloudDialogCtrl',
                backdrop:'static',
                size:'sm',
                keyboard:false,
                resolve:{
                    connect:function(){
                        return true;
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
            }).result.then(function(){
                dialogs.alert(L.settings.connectedSuccess).finally(function(){
                    window.location.reload();
                });
            },errorHandler);
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
                        mediaserver.changeAdminPassword($scope.settings.rootPassword).then(function(result){
                            resultHandler(result);
                            if($localStorage.login == Config.defaultLogin){
                                mediaserver.login($localStorage.login, $scope.settings.rootPassword);
                            }
                        }, errorHandler);
                    },function(){
                        dialogs.alert(L.settings.wrongPassword);
                    });
                });
        };

        $scope.disconnectFromSystem = function(){ // Detach server from the system
            dialogs.confirmWithPassword(
                "", // L.settings.confirmDisconnectFromSystemTitle,
                L.settings.confirmDisconnectFromSystem,
                L.settings.confirmDisconnectFromSystemAction,
                'danger').then(function (oldPassword) {
                    //1. Check password
                    return mediaserver.checkCurrentPassword(oldPassword).
                        then(function(){
                            return mediaserver.disconnectFromSystem().then(function(){
                                return dialogs.alert(L.settings.disconnectedFromSystemSuccess).
                                    finally(function(){
                                        window.location.reload();
                                    });
                            }, function(error){
                                dialogs.alert(L.settings.unexpectedError);
                                console.error(error);
                            });
                        },function(){
                            dialogs.alert(L.settings.wrongPassword);
                        });
                });
        };

        $scope.disconnectFromCloud = function() { // Disconnect from Cloud
            function doDisconnect(localLogin,localPassword){
                // 2. Send request to the system only
                return mediaserver.disconnectFromCloud(localLogin, localPassword).then(function(){
                    dialogs.alert(L.settings.disconnectedFromCloudSuccess.replace("{{CLOUD_NAME}}",
                                  Config.cloud.productName)).finally(function(){
                                      window.location.reload();
                                  });
                }, function(error){
                    dialogs.alert(L.settings.unexpectedError);
                    console.error(error);
                });
            }
            dialogs.confirmWithPassword(
                L.settings.confirmDisconnectFromCloudTitle.replace("{{CLOUD_NAME}}",Config.cloud.productName),
                L.settings.confirmDisconnectFromCloud.replace("{{CLOUD_NAME}}",Config.cloud.productName),
                L.settings.confirmDisconnectFromCloudAction.replace("{{CLOUD_NAME}}",Config.cloud.productName),
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
                            dialogs.createUser( null /*L.settings.createLocalOwner*/, L.settings.createLocalOwnerTitle).then(function(data){
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
            openCloudDialog(); //Open Connect Dialog
        };
        $scope.saveNetworkSettings = function(){
            mediaserver.networkSettings($scope.networkSettings).then(restartServer, errorHandler);
        };

        mediaserver.getTimeZones().then(function(reply){

            var zones = reply.data.reply;
            zones = _.sortBy(zones,function(zone){

                var offsetString = '(UTC)';
                if(zone.offsetFromUtc != 0){
                    offsetString = zone.offsetFromUtc > 0?'(UTC +':'(UTC -';

                    var absOffset = Math.abs(zone.offsetFromUtc);
                    var zoneHours = Math.floor(absOffset/3600);
                    var zoneMinutes = Math.floor( (absOffset % 3600) / 60);

                    if(zoneHours<10){
                        zoneHours = '0' + zoneHours;
                    }
                    if(zoneMinutes<10){
                        zoneMinutes = '0' + zoneMinutes;
                    }
                    offsetString += zoneHours  + ":" + zoneMinutes +")";
                }

                zone.name = offsetString +' ' + zone.id;
                return zone.offsetFromUtc;
            });
            /*$scope.alltimeZones = _.indexBy(zones,function(zone){
                return zone.id;
            });
            zones = _.uniq(zones, false, function(zone){
                return zone.comment;
            });*/
            $scope.timeZones = zones;

            return mediaserver.timeSettings().then(function(reply){
                var time = reply.data.reply;
                $scope.dateTimeSettings = {
                    timeZone: time.timezoneId,
                    dateTime: new Date(time.utcTime * 1),
                    openDate: true
                };
                /*
                var currentZone = $scope.alltimeZones[time.timezoneId];
                if($scope.timeZones.indexOf(currentZone)<=0){
                    $scope.timeZones.push(currentZone);
                }
                */
            });
        });

        $scope.openDatePicker = function($event) {
            $event.preventDefault();
            $event.stopPropagation();
            $scope.dateTimeSettings.openDate = true;
        };

        $scope.saveDateTime = function(){
            mediaserver.timeSettings($scope.dateTimeSettings.dateTime.getTime(), $scope.dateTimeSettings.timeZone).
                then(resultHandler,errorHandler);
        };
    }]);

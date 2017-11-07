'use strict';

angular.module('webadminApp')
    .controller('JoinCtrl', function ($scope, $modalInstance, $interval, mediaserver,dialogs) {
        $scope.settings = {
            url :'',
            login: Config.defaultLogin,
            password :'',
            currentPassword:'',
            keepMySystem:true
        };
        $scope.systems = {
            joinSystemName : 'NOT FOUND',
            systemName: 'SYSTEMNAME',
            systemFound: false
        };


        mediaserver.getModuleInformation().then(function (r) {
            $scope.systems.systemName =  r.data.reply.systemName;

            /*$scope.systems.discoveredUrls = _.filter($scope.systems.discoveredUrls, function(module){
                return module.systemName !== $scope.systems.systemName;
            });*/

            mediaserver.discoveredPeers().then(function (r) {
                var systems = _.map(r.data.reply, function(module)
                {
                    var isNew = module.serverFlags.indexOf(Config.newServerFlag)>=0;
                    var system = {
                        url: module.remoteAddresses[0] + ':' + module.port,
                        systemName: isNew ?  L.join.newSystemDisplayName : module.systemName,
                        ip: module.remoteAddresses[0],
                        name: module.name,
                        isNew: isNew,
                        compatibleProtocol: module.protoVersion == Config.protoVersion,
                        compatibleCloudHost: module.cloudHost == Config.cloud.host,
                        compatibleCloudState: !module.cloudSystemId || !Config.cloud.systemId
                    };
                    system.visibleName = system.systemName + ' (' + system.url + ' - ' + system.name + ')';
                    system.compatible = system.compatibleProtocol && system.compatibleCloudHost && system.compatibleCloudState;
                    if(!system.compatible) {
                        system.visibleName += ' - ' +
                        (!system.compatibleProtocol ? L.join.incompatibleProtocol:
                            !system.compatibleCloudHost ? L.join.incompatibleCloudHost:
                                !system.compatibleCloudState ? L.join.incompatibleCloudState:
                                    '');
                    }
                    return system;
                });

                systems = _.sortBy(systems,function(system){
                   return (system.compatible?'0':'1') + system.visibleName;
                });

                $scope.systems.discoveredUrls = _.filter(systems, function(module){
                    return module.systemName !== $scope.systems.systemName;
                });

            });
        });


        function errorHandler(errorToShow){
            switch(errorToShow){
                case 'FAIL':
                    errorToShow = L.join.systemIsUnreacheble;
                    break;
                case 'currentPassword':
                    errorToShow = L.join.incorrectCurrentPassword;
                    break;
                case 'UNAUTHORIZED':
                case 'password':
                    errorToShow = L.join.incorrectRemotePassword;
                    break;
                case 'INCOMPATIBLE':
                    errorToShow = L.join.incompatibleVersion;
                    break;
                case 'url':
                    errorToShow = L.join.wrongUrl;
                    break;
                case 'SAFE_MODE':
                    errorToShow =  L.join.safeMode;
                    break;

                case 'UNCONFIGURED_SYSTEM':
                    errorToShow = L.join.newSystemError;
                    break;
                case 'CONFIGURATION_ERROR':
                    errorToShow = L.join.configError;
                    break;
                case 'DEPENDENT_SYSTEM_BOUND_TO_CLOUD':
                case 'BOTH_SYSTEM_BOUND_TO_CLOUD':
                    errorToShow = L.join.cloudError;
                    break;
                case 'DIFFERENT_CLOUD_HOST':
                    errorToShow = L.join.cloudHostConflict;
                    break;
                case 'STARTER_LICENSE_ERROR':
                    errorToShow = L.join.licenceError;
                    dialogs.alert(errorToShow);
                    return false;
            }
            return errorToShow;
        }
        function normalizeUrl(url){
            if(url.indexOf(":")<0){
                url = url + ":7001";
            }
            if(url.indexOf("//")<0){
                url = "http://" + url;
            }
            return url;
        }
        $scope.test = function () {

            /*if (!$('#mergeSystemForm').valid()) {
                return;
            }*/

            var remoteUrl = normalizeUrl($scope.settings.url);
            mediaserver.pingSystem(remoteUrl, $scope.settings.login, $scope.settings.password || Config.defaultPassword).then(function(r){
                if(r.data && r.data.error!=='0'){
                    var errorToShow = errorHandler(r.data.errorString);
                    if(errorToShow){
                        dialogs.alert(L.join.connectionFailed + errorToShow);
                        return;
                    }
                }
                if(r.data.reply.cloudSystemId && Config.cloud.systemId){
                    dialogs.alert(L.join.cloudBothError);
                    return;
                }

                if(r.data.reply.cloudSystemId){
                    $scope.settings.keepMySystem = false;
                }
                if(Config.cloud.systemId){
                    $scope.settings.keepMySystem = true;
                }

                $scope.systems.systemFound = true;
                $scope.systems.joinSystemName = r.data.reply.systemName;
                $scope.systems.remoteSystemIsNew = r.data.reply.serverFlags.indexOf(Config.newServerFlag)>=0;
                if($scope.systems.remoteSystemIsNew){
                    $scope.settings.keepMySystem = true;
                    $scope.systems.joinSystemName = L.join.newSystemDisplayName;
                }

            },function(r){
                var errorToShow = L.join.unknownError;
                if(r.data && r.data.error!=='0') {
                    errorToShow = errorHandler(r.data.errorString);
                }
                dialogs.alert(L.join.connectionFailed + errorToShow);
            });
        };

        $scope.join = function () {

            /*if (!$('#mergeSustemForm').valid()) {
                return;
            }*/

            // TODO: $scope.settings.currentPassword here

            var remoteUrl = normalizeUrl($scope.settings.url);
            mediaserver.checkCurrentPassword($scope.settings.currentPassword).then(function() {
                mediaserver.mergeSystems(remoteUrl, $scope.settings.login, $scope.settings.password || Config.defaultPassword,
                    $scope.settings.keepMySystem).then(function (r) {
                        if (r.data.error !== '0') {
                            var errorToShow = errorHandler(r.data.errorString);
                            if (errorToShow) {
                                dialogs.alert(L.join.mergeFailed + errorToShow);
                                return;
                            }
                        }
                        dialogs.alert(L.join.mergeSucceed).finally(function () {
                            window.location.reload();
                        });
                    });
            },function(){
                dialogs.alert(L.settings.wrongPassword);
            });
        };

        $scope.selectSystem = function (system){
            $scope.settings.url = system.url;
            $scope.selectedSystem = system;
            if(system.isNew){
                $scope.settings.disablePassword = true;
                $scope.settings.login = Config.defaultLogin;
                $scope.settings.password = Config.defaultPassword;
            }
        };

        $scope.cancel = function () {
            $modalInstance.dismiss('cancel');
        };
    });

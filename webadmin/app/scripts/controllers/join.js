'use strict';

angular.module('webadminApp')
    .controller('JoinCtrl', function ($scope, $modalInstance, $interval, mediaserver,dialogs) {
        $scope.settings = {
            url :'',
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
                    return {
                        url: window.location.protocol + '//' + module.remoteAddresses[0] + ':' + module.port,
                        systemName: module.systemName,
                        ip: module.remoteAddresses[0],
                        name: module.name
                    };
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
                case 'CONFIGURATION_ERROR':
                    errorToShow = L.join.configError;
                    break;
                case 'STARTER_LICENSE_ERROR':
                    errorToShow = L.join.licenceError;
                    dialogs.alert(errorToShow);
                    return false;
            }
            return errorToShow;
        }

        $scope.test = function () {

            /*if (!$('#mergeSystemForm').valid()) {
                return;
            }*/

            mediaserver.pingSystem($scope.settings.url, Config.defaultLogin, $scope.settings.password).then(function(r){
                if(r.data.error!=='0'){
                    var errorToShow = errorHandler(r.data.errorString);
                    if(errorToShow){
                        dialogs.alert(L.join.connectionFailed + errorToShow);
                        return;
                    }
                }
                $scope.systems.systemFound = true;
                $scope.systems.joinSystemName = r.data.reply.systemName;
            });
        };

        $scope.join = function () {

            /*if (!$('#mergeSustemForm').valid()) {
                return;
            }*/

            mediaserver.mergeSystems($scope.settings.url,Config.defaultLogin,$scope.settings.password,
                $scope.settings.currentPassword,$scope.settings.keepMySystem).then(function(r){
                if(r.data.error!=='0') {
                    var errorToShow = errorHandler(r.data.errorString);
                    if (errorToShow) {
                        dialogs.alert(L.join.mergeFailed + errorToShow);
                        return;
                    }
                }
                dialogs.alert(L.join.mergeSucceed).then(function(){
                    window.location.reload();
                });
            });
        };

        $scope.selectSystem = function (system){
            $scope.settings.url = system.url;
        };

        $scope.cancel = function () {
            $modalInstance.dismiss('cancel');
        };
    });

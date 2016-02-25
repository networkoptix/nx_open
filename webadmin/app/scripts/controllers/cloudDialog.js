'use strict';

angular.module('webadminApp')
    .controller('CloudDialogCtrl', function ($scope, $modalInstance, mediaserver, connect, systemName) {

        //1. Detect action: connect or disconnect
        if(connect) {
            $scope.title = 'Connect system to Nx cloud';
            $scope.actionLabel = 'Connect';
            $scope.cloudPortalUrl = Config.cloud.portalConnectUrl.replace('{systemName}',encodeURIComponent(systemName));

            $scope.successMessage = 'System is connected to cloud';
            $scope.errorMessage = 'Can\'t connect to cloud: some error happened';

        }else{
            $scope.title = 'Disconnect system from Nx cloud';
            $scope.actionLabel = 'Disconnect';
            $scope.cloudPortalUrl = Config.cloud.portalDisconnectUrl.replace('{systemId}',encodeURIComponent(systemName));

            $scope.successMessage = 'System was disconnected from cloud';
            $scope.errorMessage = 'Some error happened';
        }

        function successHandler(result){
            $scope.connecting = false;
            $scope.finished = true;
            $scope.succeed = true;

            if(result.data.errorString && result.data.errorString !== ''){
                $scope.errorMessage     += ': ' + result.data.errorString;
                $scope.succeed = false;
            }
        }
        function errorHandler(result){
            $scope.connecting = false;
            $scope.finished = true;
            $scope.succeed = false;
            console.error(result);
        }

        //3. Handle portal result
        function messageHandler(message){
            switch(message.data.message){
                case 'connected':
                    mediaserver.saveCloudSystemCredentials(message.data.systemId, message.data.authKey).
                        then(successHandler,errorHandler);
                    break;
                case 'disconnected':
                    mediaserver.clearCloudSystemCredentials().then(successHandler,errorHandler);
                    break;
                default:
                    // WTF?
            }

            $scope.hideCloud = true;
            $scope.connecting = true;
            $scope.$digest();
        }
        window.addEventListener('message', messageHandler, false);

        $scope.cancel = function(){
            $modalInstance.dismiss('cancel');
        };
    });
'use strict';

angular.module('webadminApp')
    .controller('CloudDialogCtrl', ['$scope', '$uibModalInstance', 'mediaserver', 'cloudAPI', 'connect',
                                    'systemName', 'cloudSystemID', 'cloudAccountName',
    function ($scope, $uibModalInstance, mediaserver, cloudAPI, connect, systemName, cloudSystemID, cloudAccountName) {
        //1. Detect action: connect or disconnect
        $scope.connect = connect;
        $scope.Config = Config;
        if(connect) {
            $scope.title = 'Connect System to <span class="product-name">' + Config.cloud.productName + '</span>';
            $scope.actionLabel = 'Connect System';

            $scope.successMessage = 'System is connected to <span class="product-name">' + Config.cloud.productName + '</span>';
            $scope.errorMessage = 'Can\'t connect: some error happened';

        }else{
            $scope.title = 'Disconnect System from <span class="product-name">' + Config.cloud.productName + '</span>';
            $scope.actionLabel = 'Disconnect System';

            $scope.successMessage = 'System was disconnected from <span class="product-name">' + Config.cloud.productName + '</span>';
            $scope.errorMessage = 'Some error happened';
        }

        function successHandler(result){
            $scope.connecting = false;
            $scope.finished = true;
            $scope.succeed = true;

            if(result.data.errorString && result.data.errorString !== ''){
                $scope.errorMessage += ': ' + result.data.errorString;
                $scope.succeed = false;
            }else {
                $uibModalInstance.close('success');
            }
        }
        function errorHandler(result){
            $scope.connecting = false;
            $scope.finished = true;
            $scope.succeed = false;
            $scope.errorMessage = 'Some error happened';
            if(result.data.errorString && result.data.errorString !== ''){
                $scope.errorMessage += ': ' + result.data.errorString;
            }
            console.error(result);
        }

        $scope.cancel = function(){
            $uibModalInstance.dismiss('cancel');
        };


        $scope.settings = {
            cloudEmail: cloudAccountName,
            cloudPassword: '',
            systemName: systemName
        };

        function formatError(errorToShow){
            var errorMessages = {
                'FAIL':'System is unreachable or doesn\'t exist.',
                'currentPassword':'Incorrect current password',
                'UNAUTHORIZED':'Wrong password.',
                'password':'Wrong password.',
                'INCOMPATIBLE':'Remote System has incompatible version.',
                'url':'Wrong url.',
                'SAFE_MODE':'Can\'t connect to a System. Remote System is in safe mode.',
                'CONFIGURATION_ERROR':'Can\'t connect to a System. Maybe one of the Systems is in safe mode.',

                // Cloud errors:
                'notAuthorized': 'Login or password are incorrect',
                'accountNotActivated': 'Please, confirm your account first'
            };
            return errorMessages[errorToShow] || errorToShow;
        }

        function cloudErrorHandler(error){

            $scope.connecting = false;
            $scope.finished = true;
            $scope.succeed = false;
            if(error.status === 401) {
                if (error.data.resultCode) {
                    $scope.errorMessage = formatError(error.data.resultCode);
                } else {
                    $scope.errorMessage = formatError('notAuthorized');
                }
            }
        }

        $scope.action = function(){
            $scope.settings.cloudError = false;
            if(connect){
                cloudAPI.connect( $scope.settings.systemName, $scope.settings.cloudEmail, $scope.settings.cloudPassword).then(
                    function(message){
                        //2. Save settings to local server
                        mediaserver.saveCloudSystemCredentials(message.data.id, message.data.authKey,
                            $scope.settings.cloudEmail).then(successHandler,errorHandler);
                    }, cloudErrorHandler);
            }else{
                console.error("this method is obsolete and does not work anymore");
            }
        };
    }]);
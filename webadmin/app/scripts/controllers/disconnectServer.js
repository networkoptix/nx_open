'use strict';

angular.module('webadminApp')
    .controller('DisconnectServerCtrl', function ($scope, $modalInstance, $interval, mediaserver) {
        $scope.settings = {
            systemName :'',
            password :''
        };

        function errorHandler(result){
            if(result.data.errorString && result.data.errorString !== ''){
                alert('Error: ' + result.data.errorString);
                return;
            }
            alert('Error');
        }

        function resultHandler(result){
            if(result.data.errorString && result.data.errorString !== ''){
                alert('Error: ' + result.data.errorString);
                return;
            }

            mediaserver.clearCloudSystemCredentials().then(function(){
                alert('Success');
                window.location.reload();
            },errorHandler);

        }

        $scope.disconnectServer = function(){
            if($scope.form.$valid) {
                mediaserver.changeSystem($scope.settings.systemName, $scope.settings.password).then(resultHandler, errorHandler);
            }else{
                alert('form is invalid');
            }
        };

        $scope.cancel = function () {
            $modalInstance.dismiss('cancel');
        };
    });

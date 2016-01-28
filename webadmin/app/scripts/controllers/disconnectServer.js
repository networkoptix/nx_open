'use strict';

angular.module('webadminApp')
    .controller('DisconnectServerCtrl', function ($scope, $modalInstance, $interval, mediaserver) {
        $scope.settings = {
            systemName :'',
            password :''
        };

        function errorHandler(errorToShow){
            switch(errorToShow){
                case 'FAIL':
                    errorToShow = 'System is unreachable or doesn\'t exist.';
                    break;
                case 'currentPassword':
                    errorToShow = 'Incorrect current password';
                    break;
                case 'UNAUTHORIZED':
                case 'password':
                    errorToShow = 'Wrong password.';
                    break;
                case 'INCOMPATIBLE':
                    errorToShow = 'Found system has incompatible version.';
                    break;
                case 'url':
                    errorToShow = 'Wrong url.';
                    break;
                case 'SAFE_MODE':
                    errorToShow = 'Can\'t merge systems. Remote system is in safe mode.';
                    break;
                case 'CONFIGURATION_ERROR':
                    errorToShow = 'Can\'t merge systems. Maybe one of the systems is in safe mode.';
                    break;
                case 'STARTER_LICENSE_ERROR':
                    errorToShow = 'Warning: You are about to merge Systems with START licenses. As only 1 START license is allowed per System after your merge you will only have 1 START license remaining. If you understand this and would like to proceed please click Merge to continue.';
                    alert(errorToShow);
                    return false;
            }
            return errorToShow;
        }

        function resultHandler(result){
            alert('Settings saved');
            window.location.reload();
        }

        $scope.disconnectServer = function(){
            if($scope.form.$valid) {
                mediaserver.changeSystem($scope.settings.systemName, $scope.settings.password).then(resultHandler, errorHandler);
            }else{
                alert("form is invalid");
            }
        };

        $scope.cancel = function () {
            $modalInstance.dismiss('cancel');
        };
    });

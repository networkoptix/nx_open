'use strict';

angular.module('cloudApp')
    .controller('DisconnectCtrl', ['$scope', 'cloudApi', 'process', 'dialogs', '$q', 'account',
    function ($scope, cloudApi, process, dialogs, $q, account) {
        $scope.buttonText = L.sharing.shareConfirmButton;
        $scope.model = {
            password:''
        }

        var dialogSettings = dialogs.getSettings($scope);

        var systemId = dialogSettings.params.systemId;

        $scope.close = function(){
            dialogs.closeMe($scope);
        }

        $scope.disconnecting = process.init(function(){
            return cloudApi.disconnect(systemId, $scope.model.password);
        },{
            ignoreUnauthorized: true,
            errorCodes:{
                notAuthorized: L.errorCodes.passwordMismatch
            },
            successMessage: L.system.successDisconnected,
            errorPrefix:L.errorCodes.cantDisconnectSystemPrefix
        }).then(function(){
            dialogs.closeMe($scope, true);
        });

        $scope.disconnect = function(){
            $scope.deletingSystem.run();
        }

        $scope.$on('$destroy', function(){
            dialogs.dismissNotifications();
        });
    }]);

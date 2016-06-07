'use strict';

angular.module('cloudApp')
    .controller('DisconnectCtrl', ['$scope', 'cloudApi', 'process', 'dialogs', '$q', 'account',
    function ($scope, cloudApi, process, dialogs, $q, account) {

        $scope.Config = Config;
        $scope.L = L;
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
            errorCodes:{
                notAuthorized: L.errorCodes.passwordMismatch
            },
            successMessage: L.system.successDisconnected,
            errorPrefix:'Cannot delete the system:'
        }).then(function(){
            dialogs.closeMe($scope, true);
        });

        $scope.disconnect = function(){
            $scope.deletingSystem.run();
        }
    }]);
'use strict';

angular.module('cloudApp')
    .controller('RenameCtrl', ['$scope', 'cloudApi', 'process', 'dialogs', '$q', 'account',
    function ($scope, cloudApi, process, dialogs, $q, account) {

        $scope.Config = Config;
        $scope.L = L;
        $scope.buttonText = L.sharing.renameConfirmButton;

        var dialogSettings = dialogs.getSettings($scope);
        $scope.model = {
            systemName: dialogSettings.params.systemName
        }

        var dialogSettings = dialogs.getSettings($scope);

        var systemId = dialogSettings.params.systemId;

        $scope.close = function(){
            dialogs.closeMe($scope);
        }

        $scope.renaming = process.init(function(){
            return cloudApi.renameSystem(systemId, $scope.model.systemName);
        },{
            successMessage: L.system.successRename
        }).then(function(){
            dialogs.closeMe($scope, $scope.model.systemName);
        });
    }]);
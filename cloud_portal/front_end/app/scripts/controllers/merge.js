'use strict';

angular.module('cloudApp')
    .controller('MergeCtrl', ['$scope', 'cloudApi', 'process', 'dialogs', '$q', 'account', 'systemsProvider',
    function ($scope, cloudApi, process, dialogs, $q, account, systemsProvider) {
        var dialogSettings = dialogs.getSettings($scope);
        $scope.system = dialogSettings.params.system;


        $scope.mergeSystemTextTop = L.system.mergeSystemTextTop;
        $scope.mergeSystemTextBottom = L.system.mergeSystemTextBottom;
        $scope.user = null;
        $scope.systems = null;
        console.log($scope.system);

        account.get().then(function(user){
            $scope.user = user;
            $scope.systems = systemsProvider.getMySystems(user.email, $scope.system.id);
            $scope.canMerge = $scope.system.accessRole.toLowerCase() === "owner" && $scope.systems.length > 0;
            $scope.targetSystem = $scope.systems[0];
            console.log($scope.systems);
        });

        $scope.setTargetSystem = function(system){
            $scope.targetSystem = system;
        };

        $scope.close = function(){
            dialogs.closeMe($scope);
        };

        $scope.$on('$destroy', function(){
            dialogs.dismissNotifications();
        });
    }]);

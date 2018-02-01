'use strict';

angular.module('cloudApp')
    .controller('MergeCtrl', ['$scope', 'cloudApi', 'process', 'dialogs', '$q', 'account', 'systemsProvider',
    function ($scope, cloudApi, process, dialogs, $q, account, systemsProvider) {
        var dialogSettings = dialogs.getSettings($scope);
        $scope.system = dialogSettings.params.system;

        $scope.user = null;
        $scope.systems = null;

        //Add system can merge where added to systems form api call
        function checkMergeAbility(system){
            if(system.stateOfHealth == 'offline')
                return 'offline'
            return ''
        }

        account.get().then(function(user){
            $scope.user = user;
            $scope.systems = systemsProvider.getMySystems(user.email, $scope.system.id);
            $scope.canMerge = $scope.system.accessRole.toLowerCase() === "owner" && $scope.systems.length > 0;
            $scope.targetSystem = $scope.systems[0];
            $scope.systemStatus = checkMergeAbility($scope.targetSystem);
        });

        $scope.setTargetSystem = function(system){
            $scope.targetSystem = system;
            $scope.systemStatus = checkMergeAbility(system);
        };

        $scope.mergingSystems = process.init(function(){
            var masterSystemId = null;
            var slaveSystemId = null;
            if($scope.masterSystemId == $scope.system.id){
                masterSystemId = $scope.system.id;
                slaveSystemId = $scope.targetSystem.id;
            }
            else{
                masterSystemId = $scope.targetSystem.id;
                slaveSystemId = $scope.system.id;
            }
            //return cloudApi.systems(); //In for testing purposes with merging things
            return cloudApi.merge(masterSystemId, slaveSystemId);
        },{
            successMessage: L.system.mergeSystemSuccess
        }).then(function(){
            systemsProvider.forceUpdateSystems();
            dialogs.closeMe($scope, {"targetSystemId":$scope.targetSystem.id, "masterSystemId": $scope.masterSystemId});
        });

        $scope.close = function(){
            dialogs.closeMe($scope);
        };

        $scope.$on('$destroy', function(){
            dialogs.dismissNotifications();
        });
    }]);

'use strict';

angular.module('cloudApp')
    .controller('SystemsCtrl', ['$scope', 'cloudApi', '$location', 'urlProtocol', 'process',
                                'account', '$routeParams', 'systemsProvider', 'dialogs',
    function ($scope, cloudApi, $location, urlProtocol, process, account, $routeParams, systemsProvider, dialogs) {

        account.requireLogin().then(function(account){
            $scope.account = account;
            $scope.gettingSystems.run();
        });

        $scope.showSearch = false;

        $scope.systemsProvider = systemsProvider;
        $scope.$watch('systemsProvider.systems', function(){
            $scope.systems = $scope.systemsProvider.systems;
            $scope.showSearch = $scope.systems.length >= Config.minSystemsToSearch;
            if($scope.systems.length ==1){
                $scope.openSystem($scope.systems[0]);
            }
        });

        $scope.gettingSystems = process.init($scope.systemsProvider.forceUpdateSystems,{
            errorPrefix: L.errorCodes.cantGetSystemsListPrefix,
            logoutForbidden: true
        });

        $scope.openSystem = function(system){
            $location.path('/systems/' + system.id);
        };
        $scope.getSystemOwnerName = function(system){
            return systemsProvider.getSystemOwnerName(system);
        };

        $scope.search = {value:''};
        function hasMatch(str,search){
            return str.toLowerCase().indexOf(search.toLowerCase())>=0;
        }
        $scope.searchSystems = function(system){
            var search = $scope.search.value;
            return  !search ||
                    hasMatch(L.system.mySystemSearch, search) && (system.ownerAccountEmail == $scope.account.email) ||
                    hasMatch(system.name, search) ||
                    hasMatch(system.ownerFullName, search) ||
                    hasMatch(system.ownerAccountEmail, search);
        };

        $scope.$on('$destroy', function(){
            dialogs.dismissNotifications();
        });
    }]);

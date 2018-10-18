'use strict';

angular
    .module('cloudApp')
    .controller('SystemsCtrl', ['$scope', 'cloudApi', '$location', 'urlProtocol', 'process',
                                'account', '$routeParams', 'systemsProvider', 'dialogs',
                                'authorizationCheckService', 'nxConfigService', 'languageService',
    function ($scope, cloudApi, $location, urlProtocol, process,
              account, $routeParams, systemsProvider, dialogs,
              authorizationCheckService, nxConfigService, languageService) {

        $scope.Config = nxConfigService.getConfig();
        $scope.Lang = languageService.lang;

        authorizationCheckService.requireLogin().then(function(account){
            $scope.account = account;
            $scope.gettingSystems.run();
        });

        $scope.showSearch = false;

        $scope.systemsProvider = systemsProvider;
        $scope.$watch('systemsProvider.systems', function(){
            $scope.systems = $scope.systemsProvider.systems;

            $scope.showSearch = $scope.systems.length >= $scope.Config.minSystemsToSearch;
            if($scope.systems.length === 1){
                $scope.openSystem($scope.systems[0]);
            }
        });

        $scope.gettingSystems = process.init(function () {
            return systemsProvider.forceUpdateSystems();
        }, {
            errorPrefix: $scope.Lang.errorCodes.cantGetSystemsListPrefix,
            logoutForbidden: true
        });

        $scope.openSystem = function(system){
            $location.path('/systems/' + system.id);
        };
        $scope.getSystemOwnerName = function(system, currentEmail){
            return systemsProvider.getSystemOwnerName(system, currentEmail);
        };

        $scope.search = {value:''};
        function hasMatch(str,search){
            return str.toLowerCase().indexOf(search.toLowerCase())>=0;
        }
        $scope.searchSystems = function(system){
            var search = $scope.search.value;
            return  !search ||
                    hasMatch($scope.Lang.system.mySystemSearch, search) && (system.ownerAccountEmail === $scope.account.email) ||
                    hasMatch(system.name, search) ||
                    hasMatch(system.ownerFullName, search) ||
                    hasMatch(system.ownerAccountEmail, search);
        };
    }]);

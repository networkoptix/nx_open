'use strict';

angular
    .module('cloudApp')
    .controller('SystemsCtrl', ['$scope', '$filter', 'cloudApi', '$location', 'urlProtocol', 'process',
                                'account', '$routeParams', 'systemsProvider', 'dialogs',
                                'authorizationCheckService', 'configService', 'languageService',
    function ($scope, $filter, cloudApi, $location, urlProtocol, process,
              account, $routeParams, systemsProvider, dialogs,
              authorizationCheckService, configService, languageService) {

        $scope.Config = configService.config;
        $scope.Lang = languageService.lang;
        $scope.showSearch = false;
        $scope.search = { value: '' };

        function hasMatch(str, search) {
            return str.toLowerCase().indexOf(search.toLowerCase()) >= 0;
        }

        function filterSystems () {
            if ($scope.showSearch && $scope.search.value) {
                $scope.systemsSet = $filter('filter')($scope.systemsProvider.systems, function (system) {
                    if (hasMatch($scope.Lang.system.mySystemSearch, $scope.search.value) && (system.ownerAccountEmail === $scope.account.email) ||
                        hasMatch(system.name, $scope.search.value) ||
                        hasMatch(system.ownerFullName, $scope.search.value) ||
                        hasMatch(system.ownerAccountEmail, $scope.search.value)) {

                        return system;
                    }
                })
            } else {
                $scope.systemsSet = $scope.systems;
            }
        }

        authorizationCheckService.requireLogin().then(function(account){
            $scope.account = account;
            $scope.gettingSystems.run();
        });

        $scope.systemsProvider = systemsProvider;
        $scope.$watch('systemsProvider.systems', function(){
            if($scope.systems.length === 1){
                $scope.openSystem($scope.systems[0]);
                return;
            }

            $scope.systems = $scope.systemsProvider.systems;
            $scope.showSearch = $scope.systems.length >= $scope.Config.minSystemsToSearch;

            filterSystems();

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

        $scope.$watch('search.value', function () {
            filterSystems();
        });
    }]);

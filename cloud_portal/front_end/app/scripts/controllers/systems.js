'use strict';

angular.module('cloudApp')
    .controller('SystemsCtrl', ['$scope', 'cloudApi', '$location', 'urlProtocol', 'process', 'account', '$poll', '$routeParams',
    function ($scope, cloudApi, $location, urlProtocol, process, account, $poll, $routeParams) {

        account.requireLogin().then(function(account){
            $scope.account = account;
            $scope.gettingSystems.run();
        });

        $scope.showSearch = false;

        function delayedUpdateSystems(){
            var pollingSystemsUpdate = $poll(function(){
                return cloudApi.systems().then(function(result){
                    $scope.systems = cloudApi.sortSystems(result.data);

                    $scope.showSearch = $scope.systems.length >= Config.minSystemsToSearch;
                    return $scope.systems;
                });
            },Config.updateInterval);

            $scope.$on('$destroy', function( event ) {
                $poll.cancel(pollingSystemsUpdate);
            } );
        }


        $scope.gettingSystems = process.init(function(){
            return cloudApi.systems();
        },{
            errorPrefix: L.errorCodes.cantGetSystemsListPrefix,
            logoutForbidden: true
        }).then(function(result){
            // Special mode - user will be redirected to default system if default system can be determined (if user has one system)
            if(result.data.length == 1){
                $scope.openSystem(result.data[0]);
                return;
            }
            $scope.systems = cloudApi.sortSystems(result.data);
            $scope.showSearch = $scope.systems.length >= Config.minSystemsToSearch;
            delayedUpdateSystems();
        });

        $scope.openSystem = function(system){
            $location.path('/systems/' + system.id);
        };
        $scope.getSystemOwnerName = function(system){
            return cloudApi.getSystemOwnerName(system);
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
        }
    }]);
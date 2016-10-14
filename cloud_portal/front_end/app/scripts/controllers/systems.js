'use strict';

angular.module('cloudApp')
    .controller('SystemsCtrl', ['$scope', 'cloudApi', '$location', 'urlProtocol', 'process', 'account', '$poll', '$routeParams',
    function ($scope, cloudApi, $location, urlProtocol, process, account, $poll, $routeParams) {

        account.requireLogin().then(function(account){
            $scope.account = account;
        });

        $scope.Config = Config;

        function sortSystems(systems){
            // Alphabet sorting
            var preSort =  _.sortBy(systems,function(system){
                return $scope.getSystemOwnerName(system,true);
            });
            // Sort by usage frequency is more important than Alphabet
            return _.sortBy(preSort,function(system){
                return -system.usageFrequency;
            });
        }

        function delayedUpdateSystems(){
            var pollingSystemsUpdate = $poll(function(){
                return cloudApi.systems().then(function(result){
                    return $scope.systems = sortSystems(result.data);
                });
            },Config.updateInterval);

            $scope.$on("$destroy", function( event ) {
                $poll.cancel(pollingSystemsUpdate);
            } );
        }


        $scope.gettingSystems = process.init(function(){
            return cloudApi.systems();
        },{
            errorPrefix: 'Systems list is unavailable:'
        }).then(function(result){
            // Special mode - user will be redirected to default system if default system can be determined (if user has one system)
            if($routeParams.defaultMode && result.data.length == 1){
                $scope.openSystem(result.data[0]);
                return;
            }
            $scope.systems = sortSystems(result.data);
            delayedUpdateSystems();
        });
        $scope.gettingSystems.run();

        $scope.openSystem = function(system){
            $location.path('/systems/' + system.id);
        };

        $scope.getSystemOwnerName = function(system, forOrder) {
            if($scope.account && system.ownerAccountEmail == $scope.account.email ){
                if(forOrder){
                    return '!!!!!!!'; // Force my systems to be first
                }
                return L.system.yourSystem;
            }

            if(system.ownerFullName && system.ownerFullName.trim() != ''){
                return system.ownerFullName;
            }

            return system.ownerAccountEmail;
        }

        $scope.search = '';
        function hasMatch(str,search){
            return str.toLowerCase().indexOf(search.toLowerCase())>=0;
        }
        $scope.searchSystems = function(system){
            var search = $scope.search;
            return  $scope.search == '' ||
                    hasMatch(L.system.mySystemSearch, search) && (system.ownerAccountEmail == $scope.account.email) ||
                    hasMatch(system.name, search) ||
                    hasMatch(system.ownerFullName, search) ||
                    hasMatch(system.ownerAccountEmail, search);
        }
    }]);
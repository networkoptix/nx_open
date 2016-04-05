'use strict';

angular.module('cloudApp')
    .controller('SystemsCtrl', function ($scope, cloudApi, $location, urlProtocol, process, account) {

        account.requireLogin().then(function(account){
            $scope.account = account;
        });

        $scope.Config = Config;

        function sortSystems(systems){
            return _.sortBy(systems,function(system){
                var statusOrder = Config.systemStatuses.sortOrder.indexOf(system.status);
                if(statusOrder < 0){ // unknown yet status
                    statusOrder = Config.systemStatuses.sortOrder.length;
                }

                statusOrder = "0" + statusOrder;
                statusOrder = statusOrder.substr(statusOrder.length - 2); // Make leading zeros and fixed length

                var nameOrder = $scope.getSystemOwnerName(system,true);

                return statusOrder + nameOrder;

            });
        }
        $scope.gettingSystems = process.init(function(){
            return cloudApi.systems();
        },{
            errorPrefix: 'Systems list is unavailable:'
        }).then(function(result){
            $scope.systems = sortSystems(result.data);
        });
        $scope.gettingSystems.run();


        $scope.openClient = function(system,$event){
            urlProtocol.open(system?system.id:null);
            $event.stopPropagation();
        };

        $scope.openSystem = function(system){
            $location.path('/systems/' + system.id);
        };

        $scope.getSystemOwnerName = function(system, forOrder) {
            if(system.ownerAccountEmail == $scope.account.email ){
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

    });
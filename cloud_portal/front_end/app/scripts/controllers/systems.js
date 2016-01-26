'use strict';

angular.module('cloudApp')
    .controller('SystemsCtrl', function ($scope, cloudApi, $sessionStorage, $location, nativeClient, process) {

        cloudApi.account().then(function(account){
            $scope.account = account;

            if(!account){
                $location.path('/');
            }
        });

        $scope.Config = Config;

        $scope.gettingSystems = process.init(function(){
            return cloudApi.systems();
        }).then(function(result){
            $scope.systems = result.data;
        });
        $scope.gettingSystems.run();


        $scope.openClient = function(system){
            nativeClient.open(system?system.id:null);
        };

        $scope.openSystem = function(system){
            $location.path('/system/' + system.id);
        };
    });
'use strict';

angular.module('cloudApp')
    .controller('SystemsCtrl', function ($scope, cloudApi, $sessionStorage, $location, nativeClient, process, account) {

        account.requireLogin().then(function(account){
            $scope.account = account;
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
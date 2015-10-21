'use strict';

angular.module('cloudApp')
    .controller('SystemsCtrl', function ($scope,cloudApi,$sessionStorage) {
        cloudApi.systems().then(function(result){
            console.log("systems!");
            $scope.systems = result.data;
        });
        cloudApi.account().then(function(account){
            $scope.account = account;
        });

        $scope.open = function(system){
            var username = encodeURIComponent($scope.account.email);
            var password = encodeURIComponent($sessionStorage.password);
            var system   = system.id;
            var protocol = Config.clientProtocol;

            var url = protocol + username + ":" + password + "@" + system + "/";
            window.open(url);
        }
    });
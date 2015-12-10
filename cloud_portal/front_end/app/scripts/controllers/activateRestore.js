'use strict';

angular.module('cloudApp')
    .controller('ActivateRestoreCtrl', function ($scope, cloudApi, $routeParams, process) {

        $scope.data = {
            newPassword: '123',
            email: 'a@b.c',
            restoreCode: $routeParams.restoreCode,
            activateCode: $routeParams.activateCode
        };

        $scope.change = process.init(function(){
            console.log("change password", $scope.data.restoreCode, $scope.data.newPassword, $scope);
            return cloudApi.restorePassword($scope.data.restoreCode, $scope.data.newPassword);
        });

        $scope.directChange = function(){
            $scope.$apply();
            console.log("change password", $scope.data.restoreCode, $scope.data.newPassword, $scope);
            return cloudApi.restorePassword($scope.data.restoreCode, $scope.data.newPassword);
        };

        $scope.restore = process.init(function(){
            console.log("restore password", $scope.data.email, $scope);
            return cloudApi.restorePasswordRequest($scope.data.email);
        });

        $scope.activate = process.init(function(){
            return cloudApi.activate($scope.data.activateCode);
        });

        if($scope.activateCode){
            $scope.activate.run();
        }
    });

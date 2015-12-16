'use strict';

angular.module('cloudApp')
    .controller('ActivateRestoreCtrl', function ($scope, cloudApi, $routeParams,  process) {

        $scope.data = {
            newPassword: '',
            email: '',
            restoreCode: $routeParams.restoreCode,
            activateCode: $routeParams.activateCode
        };

        $scope.reactivating = $routeParams.reactivating;
        $scope.restoring = $routeParams.restoring;

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
            return cloudApi.restorePasswordRequest($scope.data.email);
        });

        $scope.activate = process.init(function(){
            return cloudApi.activate($scope.data.activateCode);
        });


        $scope.reactivate = process.init(function(){
            return cloudApi.reactivate($scope.data.email);
        });

        if($scope.data.activateCode){
            $scope.activate.run();
        }

    });

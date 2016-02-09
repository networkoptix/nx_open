'use strict';

angular.module('cloudApp')
    .controller('ActivateRestoreCtrl', function ($scope, cloudApi, $routeParams, process, $sessionStorage, account) {

        $scope.session = $sessionStorage;

        $scope.Config = Config;

        $scope.data = {
            newPassword: '',
            email: $sessionStorage.email,
            restoreCode: $routeParams.restoreCode,
            activateCode: $routeParams.activateCode
        };

        if(!$scope.data.activateCode){
            account.redirectAuthorised();
        }

        $scope.reactivating = $routeParams.reactivating;
        $scope.restoring = $routeParams.restoring;

        $scope.change = process.init(function(){
            return cloudApi.restorePassword($scope.data.restoreCode, $scope.data.newPassword);
        },{
            notFound: L.errorCodes.wrongCode,
            notAuthorized: L.errorCodes.wrongCode
        });

        $scope.directChange = function(){
            return cloudApi.restorePassword($scope.data.restoreCode, $scope.data.newPassword);
        };

        $scope.restore = process.init(function(){
            return cloudApi.restorePasswordRequest($scope.data.email);
        },{
            notFound: L.errorCodes.emailNotFound
        });

        $scope.activate = process.init(function(){
            return cloudApi.activate($scope.data.activateCode);
        },{
            notFound: L.errorCodes.wrongCode,
            notAuthorized: L.errorCodes.wrongCode
        });


        $scope.reactivate = process.init(function(){
            return cloudApi.reactivate($scope.data.email);
        },{
            forbidden: L.errorCodes.accountAlreadyActivated,
            notFound: L.errorCodes.emailNotFound
        });

        if($scope.data.activateCode){
            $scope.activate.run();
        }

    });

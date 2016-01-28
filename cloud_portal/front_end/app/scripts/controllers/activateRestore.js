'use strict';

angular.module('cloudApp')
    .controller('ActivateRestoreCtrl', function ($scope, cloudApi, $routeParams, process, $sessionStorage, account) {

        if(!$scope.data.activateCode){
            account.redirectAuthorised();
        }

        $scope.session = $sessionStorage;

        $scope.Config = Config;

        $scope.data = {
            newPassword: '',
            email: $sessionStorage.email,
            restoreCode: $routeParams.restoreCode,
            activateCode: $routeParams.activateCode
        };

        $scope.reactivating = $routeParams.reactivating;
        $scope.restoring = $routeParams.restoring;

        $scope.change = process.init(function(){
            return cloudApi.restorePassword($scope.data.restoreCode, $scope.data.newPassword);
        },{
            notFound: Config.errorCodes.wrongCode,
            notAuthorized: Config.errorCodes.wrongCode
        });

        $scope.directChange = function(){
            return cloudApi.restorePassword($scope.data.restoreCode, $scope.data.newPassword);
        };

        $scope.restore = process.init(function(){
            return cloudApi.restorePasswordRequest($scope.data.email);
        },{
            notFound: Config.errorCodes.emailNotFound
        });

        $scope.activate = process.init(function(){
            return cloudApi.activate($scope.data.activateCode);
        },{
            notFound: Config.errorCodes.wrongCode,
            notAuthorized: Config.errorCodes.wrongCode
        });


        $scope.reactivate = process.init(function(){
            return cloudApi.reactivate($scope.data.email);
        },{
            forbidden: Config.errorCodes.accountAlreadyActivated,
            notFound: Config.errorCodes.emailNotFound
        });

        if($scope.data.activateCode){
            $scope.activate.run();
        }

    });

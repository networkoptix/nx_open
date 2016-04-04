'use strict';

angular.module('cloudApp')
    .controller('ActivateRestoreCtrl', function ($scope, cloudApi, $routeParams, process, $localStorage, account, $location) {

        $scope.session = $localStorage;

        $scope.Config = Config;

        $scope.data = {
            newPassword: '',
            email: $scope.session.email,
            restoreCode: $routeParams.restoreCode,
            activateCode: $routeParams.activateCode
        };

        $scope.reactivating = $routeParams.reactivating;
        $scope.activationSuccess = $routeParams.activationSuccess;
        $scope.restoring = $routeParams.restoring;

        if($scope.reactivating){
            account.redirectAuthorised();
        }
        if($scope.data.restoreCode){
            account.logoutAuthorised();
        }


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
        $scope.activate.then(function(){
           $location.path("/activate/success", false); // Change url, do not reload
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

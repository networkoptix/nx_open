'use strict';

angular.module('cloudApp')
    .controller('ActivateRestoreCtrl',['$scope', 'cloudApi', '$routeParams', 'process', '$localStorage', 'account', '$location', 'urlProtocol', function ($scope, cloudApi, $routeParams, process, $localStorage, account, $location, urlProtocol) {

        $scope.session = $localStorage;

        $scope.Config = Config;

        $scope.data = {
            newPassword: '',
            email: account.getEmail(),
            restoreCode: $routeParams.restoreCode,
            activateCode: $routeParams.activateCode
        };

        $scope.reactivating = $routeParams.reactivating;
        $scope.reactivatingSuccess = $routeParams.reactivatingSuccess;
        $scope.activationSuccess = $routeParams.activationSuccess;
        $scope.changeSuccess = $routeParams.changeSuccess;
        $scope.restoringSuccess = $routeParams.changeSuccess;
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
            errorCodes:{
                notFound: L.errorCodes.wrongCode,
                notAuthorized: L.errorCodes.wrongCode
            },
            holdAlerts:true,
            errorPrefix:'Couldn\'t save new password:'
        }).then(function(){
            $location.path("/restore_password/success", false); // Change url, do not reload
        });

        $scope.directChange = function(){
            return cloudApi.restorePassword($scope.data.restoreCode, $scope.data.newPassword);
        };

        $scope.restore = process.init(function(){
            return cloudApi.restorePasswordRequest($scope.data.email);
        },{
            errorCodes:{
                notFound: L.errorCodes.emailNotFound
            },
            holdAlerts:true,
            errorPrefix:'Couldn\'t send confirmation email:'
        }).then(function(){
            $scope.restoring = false;
            $scope.restoringSuccess = true;
            $location.path("/restore_password/sent", false); // Change url, do not reload
        });

        $scope.openClient = function(){
            urlProtocol.open();
        };

        $scope.activate = process.init(function(){
            return cloudApi.activate($scope.data.activateCode);
        },{
            errorCodes:{
                notFound: L.errorCodes.wrongCode,
                notAuthorized: L.errorCodes.wrongCode
            },
            errorPrefix:'Couldn\'t activate your account:'
        }).then(function(){
            $location.path("/activate/success", false); // Change url, do not reload
        });


        $scope.reactivate = process.init(function(){
            return cloudApi.reactivate($scope.data.email);
        },{
            errorCodes:{
                forbidden: L.errorCodes.accountAlreadyActivated,
                notFound: L.errorCodes.emailNotFound
            },
            holdAlerts:true,
            errorPrefix:'Couldn\'t send confirmation email:'
        }).then(function(){
            $location.path("/activate/send", false); // Change url, do not reload
            $scope.reactivating = false;
            $scope.reactivatingSuccess = true;
        });

        if($scope.data.activateCode){
            $scope.activate.run();
        }

    }]);

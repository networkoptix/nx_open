'use strict';

angular.module('cloudApp')
    .controller('ActivateRestoreCtrl', function ($scope, cloudApi, $routeParams, process, $localStorage, account, $location, urlProtocol) {

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
        $scope.changeSuccess = $routeParams.changeSuccess;
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
            successMessage: 'Confirmation link was sent to your email. Check it for creating a new password',
            errorPrefix:'Couldn\'t send confirmation email:'

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

            if($scope.session.fromClient){
                urlProtocol.open();
            }
        });


        $scope.reactivate = process.init(function(){
            return cloudApi.reactivate($scope.data.email);
        },{
            errorCodes:{
                forbidden: L.errorCodes.accountAlreadyActivated,
                notFound: L.errorCodes.emailNotFound
            },
            holdAlerts:true,
            successMessage:'Confirmation link was sent to your email. Check it to continue.',
            errorPrefix:'Couldn\'t send confirmation email:'
        });

        if($scope.data.activateCode){
            $scope.activate.run();
        }

    });

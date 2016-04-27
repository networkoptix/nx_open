'use strict';

angular.module('cloudApp')
    .controller('ActivateRestoreCtrl',['$scope', 'cloudApi', '$routeParams', 'process', '$localStorage',
        '$sessionStorage', 'account', '$location', 'urlProtocol',
        function ($scope, cloudApi, $routeParams, process, $localStorage,
                  $sessionStorage, account, $location, urlProtocol) {

            $scope.session = $localStorage;
            $scope.context = $sessionStorage;

            $scope.Config = Config;

            $scope.data = {
                newPassword: '',
                email: account.getEmail(),
                restoreCode: $routeParams.restoreCode,
                activateCode: $routeParams.activateCode
            };

            $scope.reactivating = $routeParams.reactivating;
            $scope.restoring = $routeParams.restoring;

            $scope.reactivatingSuccess = $routeParams.reactivatingSuccess;
            $scope.activationSuccess = $routeParams.activationSuccess;
            $scope.restoringSuccess = $routeParams.changeSuccess;
            $scope.changeSuccess = $routeParams.changeSuccess;


            if($scope.reactivating){
                account.redirectAuthorised();
            }
            if($scope.data.restoreCode || $scope.data.activateCode){
                account.logoutAuthorised();
            }

            function setContext(name){
                $scope.context.process = name
            }

            function checkContext(name, flag){
                if(!flag){
                    return false;
                }
                if( $scope.context.process !== name ){
                    account.redirectToHome();
                }
                return true;
            }

            // Check session context
            if( checkContext("activateSuccess",  $routeParams.activationSuccess) ||
                checkContext("reactivatingSuccess",$routeParams.reactivatingSuccess) ||
                checkContext("restoringSuccess", $routeParams.restoringSuccess) ||
                checkContext("changeSuccess",    $routeParams.changeSuccess)){
                setContext(null);
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
                setContext("changeSuccess");
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
                setContext("restoringSuccess");
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
                setContext("activateSuccess");
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

                setContext("reactivatingSuccess");
                $location.path("/activate/send", false); // Change url, do not reload
                $scope.reactivating = false;
                $scope.reactivatingSuccess = true;
            });

            if($scope.data.activateCode){
                $scope.activate.run();
            }

        }]);

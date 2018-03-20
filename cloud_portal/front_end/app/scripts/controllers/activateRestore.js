'use strict';

angular.module('cloudApp')
    .controller('ActivateRestoreCtrl',['$scope', 'cloudApi', '$routeParams', 'process', '$localStorage',
        '$sessionStorage', 'accountService', '$location', 'urlProtocol', 'dialogs',
        function ($scope, cloudApi, $routeParams, process, $localStorage,
                  $sessionStorage, accountService, $location, urlProtocol, dialogs) {

            $scope.session = $localStorage;
            $scope.context = $sessionStorage;

            $scope.data = {
                newPassword: '',
                email: accountService.getEmail(),
                restoreCode: $routeParams.restoreCode,
                activateCode: $routeParams.activateCode
            };

            $scope.reactivating = $routeParams.reactivating;
            $scope.restoring = $routeParams.restoring;

            $scope.activationSuccess = $routeParams.activationSuccess;
            $scope.restoringSuccess = $routeParams.restoringSuccess;
            $scope.changeSuccess = $routeParams.changeSuccess;


            if($scope.reactivating){
                accountService.redirectAuthorised();
            }
            function checkActivate(){
                if($scope.data.activateCode){
                    setContext(null);
                    $scope.activate.run();
                }
            }
            function init(){
                if($scope.data.restoreCode || $scope.data.activateCode){
                    accountService.logoutAuthorised();
                    var code = $scope.data.restoreCode || $scope.data.activateCode;
                    accountService.checkCode(code).then(function(registered){
                        if(!registered){
                            // send to registration form with the code
                            $location.path('/register/' + code);
                        }else{
                            checkActivate();
                        }
                    },function(){
                        // Wrong activation code or some error - do nothing, keep user on this page
                        checkActivate();
                    });
                }
            }

            function setContext(name){
                $scope.context.process = name
            }

            function checkContext(name, flag){
                if(!flag){
                    return false;
                }
                if( $scope.context.process !== name ){
                    accountService.redirectToHome();
                }
                return true;
            }

            // Check session context
            if( checkContext('activateSuccess',  $routeParams.activationSuccess) ||
                checkContext('restoringSuccess', $routeParams.restoringSuccess) ||
                checkContext('changeSuccess',    $routeParams.changeSuccess)){
                setContext(null);
            }

            $scope.$on('$destroy', function(){
                dialogs.dismissNotifications();
            });

            $scope.change = process.init(function(){
                return cloudApi.restorePassword($scope.data.restoreCode, $scope.data.newPassword);
            },{
                errorCodes:{
                    notFound: L.errorCodes.wrongCodeRestore,
                    notAuthorized: L.errorCodes.wrongCodeRestore
                },
                ignoreUnauthorized: true,
                holdAlerts:true,
                errorPrefix:L.errorCodes.cantChangePasswordPrefix
            }).then(function(){
                setContext('changeSuccess');
                dialogs.dismissNotifications();
                $location.path('/restore_password/success', false); // Change url, do not reload
            });

            $scope.restore = process.init(function(){
                return cloudApi.restorePasswordRequest($scope.data.email);
            },{
                errorCodes:{
                    notFound: L.errorCodes.emailNotFound
                },
                ignoreUnauthorized: true,
                holdAlerts:true,
                errorPrefix:L.errorCodes.cantSendActivationPrefix
            }).then(function(){
                $scope.restoring = false;
                $scope.restoringSuccess = true;
                setContext('restoringSuccess');
                dialogs.dismissNotifications();
                $location.path('/restore_password/sent', false); // Change url, do not reload
            });

            $scope.openClient = function(){
                urlProtocol.open();
            };

            $scope.activate = process.init(function(){
                return cloudApi.activate($scope.data.activateCode);
            },{
                errorCodes:{
                    notFound: function(){
                        $scope.activationSuccess = false;
                        return false;
                    },
                    notAuthorized: function(){
                        $scope.activationSuccess = false;
                        return false;
                    },
                },
                errorPrefix:L.errorCodes.cantActivatePrefix
            }).then(function(){
                setContext('activateSuccess');
                $scope.activationSuccess = true;
                dialogs.dismissNotifications();
                $location.path('/activate/success', false); // Change url, do not reload
            });


            $scope.reactivate = process.init(function(){
                return cloudApi.reactivate($scope.data.email);
            },{
                errorCodes:{
                    forbidden: L.errorCodes.accountAlreadyActivated,
                    notFound: L.errorCodes.emailNotFound
                },
                holdAlerts:true,
                errorPrefix:L.errorCodes.cantSendConfirmationPrefix
            }).then(function(){
                dialogs.notify(L.account.activationLinkSent, 'success');
            });

            init();

        }]);

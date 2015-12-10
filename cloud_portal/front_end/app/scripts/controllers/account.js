'use strict';

angular.module('cloudApp')
    .controller('AccountCtrl', function ($scope, cloudApi, $location,$sessionStorage) {

        cloudApi.account().then(function(account){
            $scope.account = account;
            if(!account){
                $location.path('/');
            }
        });

        function errorHandler(result){
            alert("some error");
            console.error(result);
        }

        $scope.session = $sessionStorage;
        $scope.password = '';
        $scope.newPassword = '';


        $scope.accountChangedSuccess = false;
        $scope.accountChangedError = false;
        $scope.passwordChangedSuccess = false;
        $scope.passwordChangedError = false;


        $scope.save = function(){

            $scope.accountChanging = true;
            cloudApi.accountPost($scope.account.first_name,$scope.account.last_name,$scope.account.subscribe).then(function(){
                if($scope.password) { // TODO: This is dirty security hole, but I need this to make "Open in client" work
                    $scope.session.password = $scope.password;
                }
                $scope.accountChanging = false;
                $scope.accountChangedSuccess = true;
                $scope.accountChangedError = false;
            },function(error){
                $scope.accountChanging = false;
                $scope.accountChangedSuccess = false;
                $scope.accountChangedError = true;
            });
        };

        $scope.changePassword = function(){
            $scope.passwordChanging = true;
            cloudApi.changePassword($scope.newPassword,$scope.password).then(function(result){
                $scope.session.password = $scope.newPassword;
                $scope.passwordChangedSuccess = true;
                $scope.passwordChangedError = false;
                $scope.passwordChanging = false;
            },function(error){
                $scope.passwordChangedSuccess = false;
                $scope.passwordChangedError = true;
                $scope.passwordChanging = false;
            });
        };
    });
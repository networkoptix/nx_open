'use strict';

angular.module('cloudApp')
    .controller('AccountCtrl', function ($scope, cloudApi, process, $location,$sessionStorage) {

        cloudApi.account().then(function(account){
            $scope.account = account;
            if(!account){
                $location.path('/');
            }
        });

        $scope.session = $sessionStorage;
        $scope.password = '';
        $scope.newPassword = '';

        $scope.save = process.init(function() {
            return cloudApi.accountPost($scope.account.first_name,$scope.account.last_name,$scope.account.subscribe);
        });
        $scope.changePassword = process.init(function() {
            return cloudApi.changePassword($scope.newPassword,$scope.password);
        });
        $scope.changePassword.promise.then(function(){
            $scope.session.password = $scope.newPassword;
        });

    });
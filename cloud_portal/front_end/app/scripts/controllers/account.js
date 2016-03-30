'use strict';

angular.module('cloudApp')
    .controller('AccountCtrl', function ($scope, cloudApi, process, $localStorage, $routeParams, account) {

        account.requireLogin().then(function(account){
            $scope.account = account;
        });

        $scope.accountMode = $routeParams.accountMode;
        $scope.passwordMode =  $routeParams.passwordMode;

        $scope.session = $localStorage;

        $scope.pass = {
            password: '',
            newPassword: ''
        };

        $scope.save = process.init(function() {
            return cloudApi.accountPost($scope.account.first_name,$scope.account.last_name,$scope.account.subscribe);
        });
        $scope.changePassword = process.init(function() {
            return cloudApi.changePassword($scope.pass.newPassword,$scope.pass.password);
        },{
            notAuthorized: L.errorCodes.oldPasswordMistmatch
        }).then(function(){
            $scope.session.password = $scope.newPassword;
        });

    });
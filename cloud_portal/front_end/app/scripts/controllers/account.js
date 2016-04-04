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
        },{
            successMessage:'Your account was successfully saved.',
            errorPrefix:'Couldn\'t save your data:'
        });
        $scope.changePassword = process.init(function() {
            return cloudApi.changePassword($scope.pass.newPassword,$scope.pass.password);
        },{
            errorCodes:{
                notAuthorized: L.errorCodes.oldPasswordMistmatch
            },
            successMessage:'Your password was successfully changed.',
            errorPrefix:'Couldn\'t change your password:'
        }).then(function(){
            $scope.session.password = $scope.newPassword;
        });

    });
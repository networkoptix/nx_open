'use strict';

angular.module('cloudApp')
    .controller('AccountCtrl', function ($scope, cloudApi, process, $location, $sessionStorage, $routeParams) {

        cloudApi.account().then(function(account){
            $scope.account = account;
            if(!account){
                $location.path('/');
            }
        });

        $scope.accountMode = $routeParams.accountMode;
        $scope.passwordMode =  $routeParams.passwordMode;

        $scope.session = $sessionStorage;

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
            notAuthorized: Config.errorCodes.oldPasswordMistmatch
        }).then(function(){
            $scope.session.password = $scope.newPassword;
        });

    });
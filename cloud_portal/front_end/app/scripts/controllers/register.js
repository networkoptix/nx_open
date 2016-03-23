'use strict';

angular.module('cloudApp')
    .controller('RegisterCtrl', function ($scope, cloudApi, process, $location, $sessionStorage, $routeParams, account, dialogs) {

        account.logoutAuthorised();

        $scope.Config = Config;
        $scope.session = $sessionStorage;

        var registerEmail = $routeParams.email || '';
        $scope.lockEmail = !!$routeParams.email;

        $scope.account = {
            email: registerEmail,
            password: '',
            firstName: '',
            lastName: '',
            subscribe: true
        };

        $scope.setRegisterForm = function(scope){
            $scope.registerForm = scope;
        };
        $scope.hideAlreadyExists = function(){
            $scope.registerForm.registerForm.registerEmail.$setValidity('alreadyExists',true);
        };

        $scope.register = process.init(function() {
            return cloudApi.register(
                $scope.account.email,
                $scope.account.password,
                $scope.account.firstName,
                $scope.account.lastName,
                $scope.account.subscribe);
        },{
            alreadyExists: function(error){
                $scope.registerForm.registerForm.registerEmail.$setValidity('alreadyExists',false);
                return false;
            }
        });
    });

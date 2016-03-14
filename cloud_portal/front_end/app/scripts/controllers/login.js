'use strict';

angular.module('cloudApp')
    .controller('LoginCtrl', function ($scope, cloudApi, process, $location, $sessionStorage, $routeParams, account) {

        account.redirectAuthorised();

        $scope.Config = Config;
        $scope.session = $sessionStorage;

        $scope.auth = {
            email: '',
            password: '',
            remember: true
        };

        $scope.close = function(){
            $scope.session.email = $scope.auth.email;

            // TODO: We must replace this hack with something more angular-way,
            // but I can't figure out yet, how to implement dialog service and pass parameters to controllers
            // we need something like modalInstance
            function findSettings($scope){
                return $scope.settings || $scope.$parent && findSettings($scope.$parent) || null;
            }

            var dialogSettings = findSettings($scope);

            if(dialogSettings && dialogSettings.params.getModalInstance){
                dialogSettings.params.getModalInstance().dismiss();
            }

        };

        $scope.login = process.init(function() {
            $scope.session.email = $scope.auth.email;
            return cloudApi.login($scope.auth.email, $scope.auth.password, $scope.auth.remember);
        }).then(function(){
            if($scope.auth.password) { // TODO: This is dirty security hole, but I need this to make "Open in client" work
                $scope.session.password = $scope.auth.password;
            }
            setTimeout(function(){
                document.location.reload();
            });
        });


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

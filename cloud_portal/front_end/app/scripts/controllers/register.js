'use strict';

angular.module('cloudApp')
    .run(function($http,$templateCache) {
        // Preload content into cache
        $http.get('views/static/register-intro.html', {cache: $templateCache});
    })
    .controller('RegisterCtrl', function ($scope, cloudApi, process, $location, $localStorage, $routeParams, account, dialogs) {

        account.logoutAuthorised();

        $scope.Config = Config;
        $scope.session = $localStorage;

        var registerEmail = $routeParams.email || '';
        $scope.lockEmail = !!$routeParams.email;

        $scope.registerSuccess = $routeParams.registerSuccess;

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

        $scope.register.then(function(){
            $location.path('/register/success',false);
        });
    });

'use strict';

angular.module('cloudApp')
    .run(function($http,$templateCache) {
        // Preload content into cache
        $http.get('views/static/register-intro.html', {cache: $templateCache});
    })
    .controller('RegisterCtrl', function ($scope, cloudApi, process, $location, $localStorage, $routeParams, account, urlProtocol) {

        account.logoutAuthorised();
        $scope.Config = Config;
        $scope.session = $localStorage;

        $scope.session.fromClient = urlProtocol.source.isApp;

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
            errorCodes:{
                alreadyExists: function(error){
                    $scope.registerForm.registerForm.registerEmail.$setValidity('alreadyExists',false);
                    return false;
                }
            },
            holdAlerts:true,
            errorPrefix:'Some error has happened:'
        }).then(function(){
            $location.path('/register/success',false);
        });
    });

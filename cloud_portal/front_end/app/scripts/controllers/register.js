'use strict';

angular.module('cloudApp')
    .run(['$http','$templateCache', function($http,$templateCache) {
        // Preload content into cache
        $http.get('views/static/register-intro.html', {cache: $templateCache});
    }])
    .controller('RegisterCtrl', [
        '$scope', 'cloudApi', 'process', '$location', '$localStorage', '$routeParams', 'account', 'urlProtocol',
        function ($scope, cloudApi, process, $location, $localStorage, $sessionStorage, $routeParams, account, urlProtocol) {

        account.logoutAuthorised();
        $scope.Config = Config;
        $scope.session = $localStorage;
        $scope.context = $sessionStorage;

        $scope.session.fromClient = urlProtocol.source.isApp;

        var registerEmail = $routeParams.email || '';
        $scope.lockEmail = !!$routeParams.email;

        $scope.registerSuccess = $routeParams.registerSuccess;

        if($scope.registerSuccess &&  $scope.context.process !== 'registerSuccess'){
            account.redirectToHome();
        }

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
            $scope.context.process = 'registerSuccess';
            $location.path('/register/success',false);
        });
    }]);

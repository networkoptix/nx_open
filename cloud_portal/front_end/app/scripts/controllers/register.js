'use strict';

angular.module('cloudApp')
    .run(['$http', '$templateCache', function($http, $templateCache) {
        // Preload content into cache
        $http.get(Config.viewsDir + 'static/register-intro.html', {cache: $templateCache});
    }])
    .controller('RegisterCtrl', [
        '$scope', 'cloudApi', 'process', '$location', '$localStorage', '$timeout', 'dialogs',
        '$sessionStorage', '$routeParams', 'account', 'urlProtocol', '$base64', 'authorizationCheckService',
        'languageService', 'nxPageService',

        function ($scope, cloudApi, process, $location, $localStorage, $timeout, dialogs,
                  $sessionStorage, $routeParams, account, urlProtocol, $base64, authorizationCheckService,
                  languageService, nxPageService) {

        $scope.registerSuccess = $routeParams.registerSuccess;
        $scope.activated = $routeParams.activated;
        $scope.lang = languageService.lang;
        
        if ($scope.registerSuccess) {
            nxPageService.setPageTitle($scope.lang.pageTitles.registerSuccess);
        } else {
            nxPageService.setPageTitle($scope.lang.pageTitles.register);
        }

        if(!$scope.registerSuccess){
            authorizationCheckService.logoutAuthorised();
        }else if($scope.activated){
            authorizationCheckService.redirectAuthorised();
        }

        $scope.session = $localStorage;
        $scope.context = $sessionStorage;

        $scope.session.fromClient = urlProtocol.source.isApp;

        var registerEmail = null;
        if($routeParams.code){
            var decoded = $base64.decode($routeParams.code);
            registerEmail = decoded.substring(decoded.indexOf(':') + 1);
            $scope.lockEmail = true;
        }



        if($scope.registerSuccess &&  $scope.context.process !== 'registerSuccess'){
            authorizationCheckService.redirectToHome();
        }

        $scope.account = {
            email: registerEmail || account.getEmail(),
            password: '',
            firstName: '',
            lastName: '',
            subscribe: true,
            code: $routeParams.code
        };

        $scope.setRegisterForm = function(scope){
            $scope.registerForm = scope;
        };
        $scope.hideAlreadyExists = function(){
            $scope.registerForm.registerForm.registerEmail.$setValidity('alreadyExists',true);
        };

        $scope.register = process.init(function() {
            account.setEmail($scope.account.email);
            return cloudApi.register(
                $scope.account.email,
                $scope.account.password,
                $scope.account.firstName,
                $scope.account.lastName,
                $scope.account.subscribe,
                $scope.account.code);
        },{
            errorCodes:{
                alreadyExists: function(error){
                    $scope.registerForm.registerForm.registerEmail.$setValidity('alreadyExists', false);
                    $scope.registerForm.registerForm.registerEmail.$setTouched();
                    return false;
                },
                portalError: $scope.lang.errorCodes.brokenAccount
            },
            holdAlerts:true,
            errorPrefix: $scope.lang.errorCodes.cantRegisterPrefix
        }).then(function(){
            $scope.context.process = 'registerSuccess';
            if($scope.account.code){
                $scope.activated = true;
                $location.path('/register/successActivated',false);
                authorizationCheckService.login($scope.account.email, $scope.account.password);
            }else{
                $location.path('/register/success',false);
                account.setEmail($scope.account.email);
            }
        });

        $scope.$on('$destroy', function(){
            dialogs.dismissNotifications();
        });
    }]);

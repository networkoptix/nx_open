'use strict';

angular.module('cloudApp')
    .controller('LoginCtrl', function ($scope, cloudApi, $location,$sessionStorage) {

        function errorHandler(result){
            alert("some error");
            console.error(result);
        }

        $scope.session = $sessionStorage;
        $scope.email = '';
        $scope.password = '';
        $scope.login = function(){
            cloudApi.login($scope.email,$scope.password).then(function(){

                if($scope.password) { // TODO: This is dirty security hole, but I need this to make "Open in client" work
                    $scope.session.password = $scope.password;
                }

                $location.path('/systems');
                document.location.reload();
            },errorHandler);
        };

        $scope.firstName = '';
        $scope.lastName = '';
        $scope.register = function(){
            cloudApi.
                register($scope.email,$scope.password,$scope.firstName,$scope.lastName,$scope.subscribe).
                then(function(result){
                    $location.path('/register/success');
                },errorHandler);
        };
    });
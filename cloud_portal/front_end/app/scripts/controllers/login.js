'use strict';

angular.module('cloudApp')
    .controller('LoginCtrl', function ($scope, cloudApi, $location) {

        function reloadPage(){
            document.location.reload();
        }

        function errorHandler(result){
            alert("some error");
            console.error(result);
        }

        $scope.email = '';
        $scope.password = '';
        $scope.login = function(){
            cloudApi.login($scope.email,$scope.password).then(reloadPage,errorHandler);
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
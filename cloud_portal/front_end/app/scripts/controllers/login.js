'use strict';

angular.module('cloudApp')
    .controller('LoginCtrl', function ($scope, cloudApi, process, $location, $sessionStorage) {

        cloudApi.account().then(function(account){
            if(account){
                $location.path('/systems');
            }
        });

        $scope.session = $sessionStorage;
        $scope.email = '';
        $scope.password = '';

        $scope.login = process.init(function() {
            return cloudApi.login($scope.email, $scope.password);
        });
        $scope.login.promise.then(function(){
                if($scope.password) { // TODO: This is dirty security hole, but I need this to make "Open in client" work
                $scope.session.password = $scope.password;
            }
            $location.path('/systems');
            document.location.reload();
        });

        $scope.account = {
            email:'',
            password:'',
            firstName:'',
            lastName:'',
            subscribe:true
        };

        $scope.register = process.init(function() {
            return cloudApi.register($scope.account.email,$scope.account.password,$scope.account.firstName,$scope.account.lastName,$scope.account.subscribe);
        });
    });

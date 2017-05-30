'use strict';

angular.module('cloudApp')
    .controller('ViewPageCtrl', ['$scope', 'account', 'system', '$routeParams',
    function ($scope, account, system, $routeParams) {
        account.requireLogin().then(function(account){
            $scope.account = account;
            var currentSystem = system($routeParams.systemId, account.email);
            currentSystem.updateSystemAuth().then(function(){
                $scope.system = currentSystem;
            }); // Make system login
        });

        $("body").addClass("webclient-page");
        $scope.$on('$destroy', function( event ) {
            $("body").removeClass("webclient-page");
        });
    }]);
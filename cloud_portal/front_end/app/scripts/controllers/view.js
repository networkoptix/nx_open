'use strict';

angular.module('cloudApp')
    .controller('ViewPageCtrl', ['$scope', 'account', 'system', '$routeParams', 'systemAPI', 'dialogs',
    '$location', '$q',
    function ($scope, account, system, $routeParams, systemAPI, dialogs, $location, $q) {
        var currentSystem;
        account.requireLogin().then(function(account){
            currentSystem = system($routeParams.systemId, account.email);
            currentSystem.updateSystemAuth().then(function(){
                $scope.systemReady = true;
                $scope.system = currentSystem.mediaserver;
            },function(error){
                // cannot read the system
                dialogs.notify(L.errorCodes.lostConnection.replace("{{systemName}}", currentSystem.name || L.errorCodes.thisSystem), 'warning');
                $location.path("/systems");
            });
        });

        var cancelSubscription = $scope.$on("unauthirosed_" + $routeParams.systemId,function(event,data){
             dialogs.notify(L.errorCodes.lostConnection.replace("{{systemName}}", currentSystem.name || L.errorCodes.thisSystem), 'warning');
             $location.path("/systems");
        });

        $("body").addClass("webclient-page");
        $scope.$on('$destroy', function( event ) {
            cancelSubscription();
            $("body").removeClass("webclient-page");
        });
    }]);

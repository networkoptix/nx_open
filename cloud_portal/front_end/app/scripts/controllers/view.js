'use strict';

angular.module('cloudApp')
    .controller('ViewPageCtrl', ['$scope', 'account', 'system', '$routeParams', 'systemAPI', 'dialogs',
    '$location', '$q', '$poll',
    function ($scope, account, system, $routeParams, systemAPI, dialogs, $location, $q, $poll) {
        var currentSystem;
        account.requireLogin().then(function(account){
            $scope.currentSystem = system($routeParams.systemId, account.email);
            $scope.currentSystem.getInfo().then(delayedUpdateSystemInfo);
            $scope.currentSystem.updateSystemAuth().then(function(){
                $scope.systemReady = true;
                $scope.system = $scope.currentSystem.mediaserver;
            },function(error){
                // cannot read the system
                dialogs.notify(L.errorCodes.lostConnection.replace("{{systemName}}", currentSystem.name || L.errorCodes.thisSystem), 'warning');
                $location.path("/systems");
            });
        });



        function delayedUpdateSystemInfo(){
            var pollingSystemUpdate = $poll(function(){
                return $scope.currentSystem.update();
            },Config.updateInterval);

            $scope.$on('$destroy', function( event ) {
                $poll.cancel(pollingSystemUpdate);
            });
        }

        var cancelSubscription = $scope.$on("unauthirosed_" + $routeParams.systemId,function(event,data){
             dialogs.notify(L.errorCodes.lostConnection.replace("{{systemName}}", currentSystem.name || L.errorCodes.thisSystem), 'warning');
             $location.path("/systems");
        });

        $("html").addClass("webclient-page");
        $scope.$on('$destroy', function( event ) {
            cancelSubscription();
            $("html").removeClass("webclient-page");
        });
    }]);

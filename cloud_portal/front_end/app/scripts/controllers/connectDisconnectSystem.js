'use strict';

angular.module('cloudApp')
    .controller('ConnectDisconnectCtrl', function ($scope, cloudApi, $routeParams, process, account, $location) {


        $scope.connect = $routeParams.connect;
        $scope.systemName = $routeParams.systemName;
        $scope.systemId = $routeParams.systemId;


        account.requireLogin().then(function(account){
            $scope.account = account;
            if(!$scope.connect){
                cloudApi.system( $scope.systemId).then(function(result){
                    $scope.systemName = result.data[0].name;
                },function(){
                    $scope.systemName = $scope.systemId;
                })
            }
        });

        function findWebAdmin(window){
            if(!window.parent || window.parent == window){
                return false;
            }
            return window.parent;
        }
        var webadmin = findWebAdmin(window);
        if(!webadmin){
            $location.path('/');
        }

        $scope.connecting = process.init(function(){
            return cloudApi.connect($scope.systemName);
        }).then(function(result){

            var systemId = result.data.id;
            var authKey = result.data.authKey;

            webadmin.postMessage({
                message: 'connected',
                systemId: systemId,
                authKey: authKey
            },'*');
            // Send ok to upper window
        });



        $scope.disconnecting = process.init(function(){
            return cloudApi.disconnect($scope.systemId);
        }).then(function(result){
            webadmin.postMessage({
                message: 'disconnected'
            },'*');
            // Send ok to upper window
        });

    });
'use strict';

angular.module('cloudApp')
    .controller('SystemCtrl', function ($scope, cloudApi, $sessionStorage, $routeParams, $location, nativeClient, dialogs, process) {


        $scope.Config = Config;
        var systemId = $routeParams.systemId;

        $scope.system = {
            users:[],
            info:{name:''}
        };

        // Retrieve system info
        $scope.gettingSystem = process.init(function(){
            return cloudApi.systems(systemId);
        }).then(function(result){
            $scope.system.info = result.data[0];
        });
        $scope.gettingSystem.run();

        //Retrieve users list
        $scope.gettingSystemUsers = process.init(function(){
            return cloudApi.users(systemId);
        }).then(function(result){
            var users = result.data;
            // Sort users here
            $scope.system.users = _.sortBy(users,function(user){
                return - Config.accessRolesSettings.order.indexOf(user.accessRole);
            });
        });
        $scope.gettingSystemUsers.run();

        $scope.open = function(){
            nativeClient.open(systemId);
        };

        // Unbind or unshare from me
        $scope.unbind = function(){

        };

        $scope.share = function(){
            // Call share dialog, run process inside
            dialogs.share(systemId);
        };

        $scope.editShare = function(user){
            //Pass user inside
            dialogs.share(systemId, user);
        };

        $scope.unshare = function(user){
            // Run a process of sharing
            $scope.unsharingMessage = "Permissions were removed from " + user.accountEmail;
            $scope.unsharing = process.init(function(){
                return cloudApi.unshare(systemId, user.accountEmail);
            }).then(function(){
                // Update users list
                $scope.system.users = _.without($scope.system.users, user);
            });
            $scope.unsharing.run();
        };

        if($routeParams.callShare){
            $scope.share();
        }
    });
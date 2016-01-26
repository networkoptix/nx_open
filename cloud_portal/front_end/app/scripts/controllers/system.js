'use strict';

angular.module('cloudApp')
    .controller('SystemCtrl', function ($scope, cloudApi, $sessionStorage, $routeParams, $location, nativeClient, dialogs, process) {


        $scope.Config = Config;
        var systemId = $routeParams.systemId;

        $scope.system = {
            users:[],
            info:{name:''}
        };

        cloudApi.account().then(function(account){
            $scope.account = account;

            if(!account){
                $location.path('/');
            }
        });

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

        function getSystemOwner(){
            return _.find( $scope.system.users,function(user){
                return user.accessRole == Config.accessRolesSettings.owner;
            });
        }
        // Unbind or unshare from me
        $scope.delete = function(){
            //1. Determine, if I am the owner

            var owner = getSystemOwner();

            if(owner && $scope.account.email == owner.accountEmail){

                // User is the owner. Deleting system means unbinding it and disconnecting all accounts
                dialogs.confirm("You are going to completely disconnect your system from the cloud. Are you sure?").
                    then(function(){
                        $scope.deletingSystem = process.init(function(){
                            return cloudApi.delete(systemId);
                        }).then(function(){
                            $location.path("/systems");
                        });
                        $scope.deletingSystem.run();
                    });

            }else{
                // User is not owner. Deleting means he'll lose access to it
                dialogs.confirm("You are going to disconnect your system from your account. You will lose an access for this system. Are you sure?").
                    then(function(){
                        $scope.deletingSystem = process.init(function(){
                            return cloudApi.unshare(systemId, $scope.account.email);
                        }).then(function(){
                            $location.path("/systems");
                        });
                        $scope.deletingSystem.run();
                    });
            }

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
            if($scope.account.email == user.accountEmail){
                return $scope.delete();
            }
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
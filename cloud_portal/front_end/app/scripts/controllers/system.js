'use strict';

angular.module('cloudApp')
    .controller('SystemCtrl', function ($scope, cloudApi, $routeParams, $location, nativeClient, dialogs, process, account) {


        $scope.Config = Config;
        $scope.L = L;
        var systemId = $routeParams.systemId;

        $scope.system = {
            users:[],
            info:{name:''}
        };

        function loadUsers(){
            if($scope.system.info.sharingPermissions.length) { // User can share - means he can view users
                $scope.gettingSystemUsers.run();
            }
        }
        account.requireLogin().then(function(account){
            $scope.account = account;
        });

        // Retrieve system info
        $scope.gettingSystem = process.init(function(){
            return cloudApi.system(systemId);
        }, {
            errorCodes: {
                forbidden: L.errorCodes.systemForbidden,
                notFound: L.errorCodes.systemNotFound
            },
            errorPrefix:'System info is unavailable:'
        }).then(function(result){
            $scope.system.info = result.data[0];
            $scope.isOwner = $scope.system.info.ownerAccountEmail == $scope.account.email;

            loadUsers();
        });
        $scope.gettingSystem.run();

        function cleanUrl(){
            $location.path('/systems/' + systemId, false);
        }
        //Retrieve users list
        $scope.gettingSystemUsers = process.init(function(){
            return cloudApi.users(systemId);
        },{
            errorPrefix:'Users list is unavailable:'
        }).then(function(result){
            var users = result.data;
            // Sort users here
            $scope.system.users = _.sortBy(users,function(user){
                return - Config.accessRoles.order.indexOf(user.accessRole);
            });

            if($routeParams.callShare){

                var emailWith = $routeParams.shareEmail;
                if(emailWith){
                    var user = _.find($scope.system.users, function(user){
                       return user.accountEmail == emailWith;
                    });
                    if(user) {
                        $scope.editShare(user).finally(cleanUrl);
                    }else{
                        $scope.editShare({
                            accountEmail:emailWith,
                            accessRole: Config.accessRoles.default
                        }).finally(cleanUrl);
                    }
                }else {
                    $scope.share().finally(cleanUrl);
                }
            }
        });



        $scope.open = function(){
            nativeClient.open(systemId);
        };

        function reloadSystems(){
            cloudApi.systems('clearCache').then(function(){
               $location.path('/systems');
            });
        }

        $scope.disconnect = function(){
            if($scope.isOwner ){

                // User is the owner. Deleting system means unbinding it and disconnecting all accounts
                dialogs.confirm(L.system.confirmDisconnect, L.system.confirmDisconnectTitle, L.system.confirmDisconnectAction, 'danger').
                    then(function(){
                        $scope.deletingSystem = process.init(function(){
                            return cloudApi.disconnect(systemId);
                        },{
                            successMessage: L.system.successDisconnected.replace('{systemName}', $scope.system.info.name),
                            errorPrefix:'Cannot delete the system:'
                        }).then(reloadSystems);
                        $scope.deletingSystem.run();
                    });

            }
        };

        $scope.delete= function(){
            if(!$scope.isOwner ){
                // User is not owner. Deleting means he'll lose access to it
                dialogs.confirm(L.system.confirmUnshareFromMe, L.system.confirmUnshareFromMeTitle, L.system.confirmUnshareFromMeAction, 'danger').
                    then(function(){
                        $scope.deletingSystem = process.init(function(){
                            return cloudApi.unshare(systemId, $scope.account.email);
                        },{
                            successMessage: L.system.successDeleted.replace('{systemName}', $scope.system.info.name),
                            errorPrefix:'Cannot delete the system:'
                        }).then(reloadSystems);
                        $scope.deletingSystem.run();
                    });
            }
        };

        $scope.share = function(){
            // Call share dialog, run process inside
            return dialogs.share(systemId, $scope.isOwner).then(loadUsers);
        };

        $scope.editShare = function(user){
            //Pass user inside
            return dialogs.share(systemId, $scope.isOwner, user).then(loadUsers);
        };

        $scope.unshare = function(user){
            if($scope.account.email == user.accountEmail){
                return $scope.delete();
            }
            dialogs.confirm(L.system.confirmUnshare, L.system.confirmUnshareTitle, L.system.confirmUnshareAction, 'danger').
                then(function(){
                    // Run a process of sharing
                    $scope.unsharing = process.init(function(){
                        return cloudApi.unshare(systemId, user.accountEmail);
                    },{
                        successMessage: L.system.permissionsRemoved.replace('{accountEmail}',user.accountEmail),
                        errorPrefix:'Sharing failed:'
                    }).then(function(){
                        // Update users list
                        $scope.system.users = _.without($scope.system.users, user);
                    });
                    $scope.unsharing.run();
                });
        };

    });
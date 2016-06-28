'use strict';

angular.module('cloudApp')
    .controller('SystemCtrl', ['$scope', 'cloudApi', '$routeParams', '$location', 'urlProtocol', 'dialogs', 'process',
    'account', 'mediaserver', '$q',
    function ($scope, cloudApi, $routeParams, $location, urlProtocol, dialogs, process, account, mediaserver, $q) {


        $scope.Config = Config;
        $scope.L = L;
        var systemId = $routeParams.systemId;
        var userId = null; // Id of current user in this system

        $scope.system = {
            id: systemId,
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
            $scope.gettingSystem.run();
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
            $scope.system.isOnline = $scope.system.info.stateOfHealth == Config.systemStatuses.onlineStatus;
            updateSystemState();
            $scope.isOwner = $scope.system.info.ownerAccountEmail == $scope.account.email;
            loadUsers();
        });


        function cleanUrl(){
            $location.path('/systems/' + systemId, false);
        }
        //Retrieve users list
        $scope.gettingSystemUsers = process.init(function(){
            if($scope.system.isOnline){ // Two separate cases - either we get info from the system (presuming it has actual names)
                return getUsersDataFromTheSystem(systemId).catch(function(){
                    return getUsersCachedInCloud(systemId);
                });
            }else{ // or we get old cached data from the cloud
                return getUsersCachedInCloud(systemId);
            }
        },{
            errorPrefix:'Users list is unavailable:'
        }).then(function(users){
            // Sort users here
            $scope.system.users = _.sortBy(users,function(user){
                return - Config.accessRoles.order.indexOf(user.accessRole);
            });
            // If system is reported to be online - try to get actual users list

            if($routeParams.callShare){
                $scope.share().finally(cleanUrl);
            }
        });

        $scope.open = function(){
            urlProtocol.open(systemId);
        };

        function reloadSystems(){
            cloudApi.systems('clearCache').then(function(){
               $location.path('/systems');
            });
        }

        $scope.disconnect = function(){
            if($scope.isOwner ){
                // User is the owner. Deleting system means unbinding it and disconnecting all accounts
                // dialogs.confirm(L.system.confirmDisconnect, L.system.confirmDisconnectTitle, L.system.confirmDisconnectAction, 'danger').
                dialogs.disconnect(systemId).then(reloadSystems);
            }
        };

        $scope.delete= function(){
            if(!$scope.isOwner ){
                // User is not owner. Deleting means he'll lose access to it
                dialogs.confirm(L.system.confirmUnshareFromMe, L.system.confirmUnshareFromMeTitle, L.system.confirmUnshareFromMeAction, 'danger').
                    then(function(){
                        $scope.deletingSystem = process.init(function(){
                            mediaserver.deleteUser(systemId, userId); // Try to remove me from the system directly
                            return cloudApi.unshare(systemId, $scope.account.email); // Anyway - send another request to cloud_db to remove myselft
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
            return dialogs.share($scope.isOwner, $scope.system).then(loadUsers);
        };

        $scope.editShare = function(user){
            //Pass user inside
            return dialogs.share($scope.isOwner, $scope.system, user).then(loadUsers);
        };

        $scope.unshare = function(user){
            if($scope.account.email == user.accountEmail){
                return $scope.delete();
            }
            dialogs.confirm(L.system.confirmUnshare, L.system.confirmUnshareTitle, L.system.confirmUnshareAction, 'danger').
                then(function(){
                    // Run a process of sharing
                    $scope.unsharing = process.init(function(){
                        return mediaserver.deleteUser(systemId, user.id);
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



        function normalizePermissionString(permissions){
            return permissions.split('|').sort().join('|');
        }
        _.each(Config.accessRoles.options,function(option){
            if(option.permissions){
                option.permissions = normalizePermissionString(option.permissions);
            }
        });



        function updateSystemState(){
            $scope.system.stateMessage = '';
            if(!$scope.system.isAvailable){
                $scope.system.stateMessage = L.system.unavailable;
            }
            if(!$scope.system.isOnline){
                $scope.system.stateMessage = L.system.offline;
            }
        }


        function getUsersCachedInCloud(systemId){
            return cloudApi.users(systemId).then(function(result){
                return result.data;
            })
        }

        function getUsersDataFromTheSystem(systemId){
            function processUsers(users, groups, accessRights){
                function findAccessRole(isAdmin, permissions){
                    permissions = normalizePermissionString(permissions);

                    var role = _.find(Config.accessRoles.options,function(option){
                        //console.log("check permission:", option, isAdmin, permissions, option.permissions == permissions);

                        return isAdmin && option.isAdmin || !isAdmin && option.permissions == permissions;
                    })

                    if(!role){
                        return 'custom';
                    }
                    return role.accessRole;
                }
                function isEmptyGuid(guid){
                    if(!guid){
                        return true;
                    }
                    guid = guid.replace(/[{}0\-]/gi,'');
                    return guid == '';
                }

                var groupAssoc = _.indexBy(groups,'id');
                users = _.filter(users, function(user){ return user.isCloud; });
                // var accessRightsAssoc = _.indexBy(accessRights,'userId');

                _.each(users,function(user){
                    if(!isEmptyGuid(user.groupId)){
                        user.group = groupAssoc[user.groupId];
                        //user.accessRights = accessRightsAssoc[user.groupId];
                        user.accessRole = user.group.name;
                    }else{
                        //user.accessRights = accessRightsAssoc[user.id];
                        user.accessRole = findAccessRole(user.isAdmin, user.permissions);
                    }
                    if(!user.isEnabled){
                        user.accessRole = Config.accessRoles.disabled;
                    }
                    user.accountEmail = user.email;
                });

                userId = _.find(users,function(user){
                    return user.accountEmail == $scope.account.email;
                })

                $scope.system.cacheData = {
                    groups: _.sortBy(groups,function(group){return group.name}),
                    groupAssoc: groupAssoc
                }
                return users;
            }

            var deferred = $q.defer();
            function errorHandler(error){
                console.error(error);
                deferred.reject(error);
            }
            mediaserver.getUsers(systemId).then(function(result){
                var usersList = result.data;
                mediaserver.getUserGroups(systemId).then(function(result){
                    var userGroups = result.data;
                    deferred.resolve(processUsers(usersList, userGroups));
                    $scope.system.isAvailable = true;
                    updateSystemState();
                    //mediaserver.getAccessRight(systemId).then(function(result){
                    //    var accessRights = result.data;
                    //
                    //    $scope.system.isAvailable = true;
                    //    processUsers(usersList,userGroups,accessRights);
                    //},errorHandler);
                },errorHandler);
            },errorHandler);

            return deferred.promise;
        }
    }]);
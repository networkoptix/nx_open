'use strict';

angular.module('cloudApp')
    .controller('SystemCtrl', ['$scope', 'cloudApi', '$routeParams', '$location', 'urlProtocol', 'dialogs', 'process',
    'account', 'mediaserver',
    function ($scope, cloudApi, $routeParams, $location, urlProtocol, dialogs, process, account, mediaserver) {


        $scope.Config = Config;
        $scope.L = L;
        var systemId = $routeParams.systemId;

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
            $scope.system.isOnline =  $scope.system.info.stateOfHealth == Config.systemStatuses.onlineStatus;
            $scope.isOwner = $scope.system.info.ownerAccountEmail == $scope.account.email;
            loadUsers();
        });


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

            // If system is reported to be online - try to get actual users list
            if($scope.system.isOnline){
                getUsersDataFromTheSystem(systemId);
            }

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



        function normalizePermissionString(permissions){
            return permissions.split('|').sort().join('|');
        }
        _.each(Config.accessRoles.options,function(option){
            if(option.permissions){
                option.permissions = normalizePermissionString(option.permissions);
            }
        });

        function getUsersDataFromTheSystem(systemId){
            function errorHandler(error){
                console.error(error);
            }

            function processUsers(users,groups,accessRights){
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
                var targetUsers =  $scope.system.users;

                var groupAssoc = _.indexBy(groups,'id');
                var accessRightsAssoc = _.indexBy(accessRights,'userId');
                var cloudUsersByEmail = _.indexBy(_.filter(users,function(user){ return user.isCloud; }),'email');
                // var nonCloudUsers = _.filter(users,function(user){ return !user.isCloud; });

                _.each(targetUsers,function(user){
                    user.localData = cloudUsersByEmail[user.accountEmail];
                    if(!user.localData){
                        return;
                    }

                    if(!isEmptyGuid(user.localData.groupId)){
                        user.group = groupAssoc[user.localData.groupId];
                        user.accessRights = accessRightsAssoc[user.localData.groupId];
                        user.accessRole = user.group.name;
                    }else{
                        user.accessRights = accessRightsAssoc[user.localData.id];
                        user.accessRole = findAccessRole(user.localData.isAdmin, user.localData.permissions);
                    }
                });

                $scope.system.cacheData = {
                    combinedUsers: targetUsers,
                    users: users,
                    groups: _.sortBy(groups,function(group){return group.name}),
                    groupAssoc: groupAssoc,
                    accessRights: accessRights,
                    accessRightsAssoc: accessRightsAssoc
                }
                /**
                    users = [
                    {
                        "email": "ebalashov+portal@networkoptix.com",
                        "fullName": "...",
                        "isCloud": true,
                        "id": "{10844574-7de9-44ea-bc36-9d6eb5afbb85}",
                        "isAdmin": false,
                        "isEnabled": true,
                        "name": "demo",
                        "permissions": "GlobalEditCamerasPermission|GlobalControlVideoWallPermission|GlobalViewLogsPermission|GlobalViewArchivePermission|GlobalExportPermission|GlobalViewBookmarksPermission|GlobalManageBookmarksPermission|GlobalUserInputPermission|GlobalAccessAllCamerasPermission"
                    }]

                    groups= [
                    {
                        "id": "{10844574-7de9-44ea-bc36-9d6eb5afbb85}",
                        "name": "demo",
                        "permissions": "GlobalEditCamerasPermission|GlobalControlVideoWallPermission|GlobalViewLogsPermission|GlobalViewArchivePermission|GlobalExportPermission|GlobalViewBookmarksPermission|GlobalManageBookmarksPermission|GlobalUserInputPermission|GlobalAccessAllCamerasPermission"
                    }]

                    accessRights= [{
                        userId: "{10844574-7de9-44ea-bc36-9d6eb5afbb85}",
                        resourceIds: ["{10844574-7de9-44ea-bc36-9d6eb5afbb85}"]
                    }]
                */
            }

            mediaserver.getUsers(systemId).then(function(result){
                var usersList = result.data;
                mediaserver.getUserGroups(systemId).then(function(result){
                    var userGroups = result.data;
                    mediaserver.getAccessRight(systemId).then(function(result){
                        var accessRights = result.data;

                        $scope.system.isAvailable = true;
                        processUsers(usersList,userGroups,accessRights);
                    },errorHandler);
                },errorHandler);
            },errorHandler);

        }

    }]);
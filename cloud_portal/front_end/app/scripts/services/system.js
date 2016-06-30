'use strict';

// Special service for operating with systems at high-level

angular.module('cloudApp')
    .factory('system', ['cloudApi', 'mediaserver', '$q', 'uuid2', function (cloudApi, mediaserver, $q, uuid2) {

        var systems = {};

        function system(systemId,currentUserEmail){
            this.id = systemId;
            this.users = [];
            this.isAvailable = false;
            this.isOnline = false;
            this.isMine = false;
            this.groups = [];
            this.info = {name:''};
            this.permissions = {};
            this.accessRole = '';
            
            this.currentUserEmail = currentUserEmail;
            this.updateSystemState();
        }

        system.prototype.updateSystemState = function(){
            this.stateMessage = '';
            if(!this.isAvailable){
                this.stateMessage = L.system.unavailable;
            }
            if(!this.isOnline){
                this.stateMessage = L.system.offline;
            }
        }

        system.prototype.checkPermissions = function(){
            var self = this;
            self.permissions = {};
            self.accessRole = self.info.accessRole;
            if(self.currentUserRecord){
                var role = findAccessRole(self.currentUserRecord.isAdmin, self.currentUserRecord.permissions);
                self.accessRole = role.accessRole;
                self.permissions.editAdmins = self.currentUserRecord.isAdmin;
                self.permissions.editUsers = self.currentUserRecord.isAdmin || self.currentUserRecord.permissions.indexOf(Config.accessRoles.editUserPermissionFlag>=0);
            }else{

                var role = findAccessRole(self.isMine, self.info.accessRole);
                self.accessRole = role.accessRole;
                if(self.isMine){
                    self.permissions.editUsers = true;
                    self.permissions.editAdmins = true;
                }else{
                    self.permissions.editUsers = self.info.accessRole.indexOf(Config.accessRoles.editUserAccessRoleFlag>=0);
                }
            }
        }
        system.prototype.getInfo = function(){
            if(!this.infoPromise){
                var deferred = $q.defer();
                var self = this;
                this.infoPromise = cloudApi.system(self.id).then(function(result){
                    self.info = result.data[0];
                    self.isOnline = self.info.stateOfHealth == Config.systemStatuses.onlineStatus;
                    self.isMine = self.info.ownerAccountEmail == self.currentUserEmail;

                    self.checkPermissions();

                    return self.info;
                });
            }
            return this.infoPromise;
        };

        system.prototype.getUsersCachedInCloud = function(){
            this.isAvailable = false;
            this.updateSystemState();
            return cloudApi.users(this.id).then(function(result){
                return result.data;
            })
        }


        function normalizePermissionString(permissions){
            return permissions.split('|').sort().join('|');
        }
        _.each(Config.accessRoles.options,function(option){
            if(option.permissions){
                option.permissions = normalizePermissionString(option.permissions);
            }
        });

        function findAccessRole(isAdmin, permissions){
            permissions = normalizePermissionString(permissions);

            var role = _.find(Config.accessRoles.options,function(option){
                return isAdmin && option.isAdmin || !isAdmin && option.permissions == permissions;
            })

            return role || { accessRole: 'custom' };
        }


        system.prototype.getUsersDataFromTheSystem = function(){
            var self = this;
            function processUsers(users, groups, accessRights){
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
                        user.accessRole = findAccessRole(user.isAdmin, user.permissions).accessRole;
                    }
                    if(!user.isEnabled){
                        user.accessRole = Config.accessRoles.disabled;
                    }

                    user.accountEmail = user.email;

                    if(user.accountEmail == self.currentUserEmail){
                        self.currentUserRecord = user;
                        self.checkPermissions();
                    }
                });

                self.groups = _.sortBy(groups,function(group){return group.name});
                return users;
            }

            var deferred = $q.defer();
            function errorHandler(error){
                console.error(error);
                deferred.reject(error);
            }
            mediaserver.getUsers(self.id).then(function(result){
                var usersList = result.data;
                mediaserver.getUserGroups(self.id).then(function(result){
                    var userGroups = result.data;
                    deferred.resolve(processUsers(usersList, userGroups));
                    self.isAvailable = true;
                    self.updateSystemState();
                    //mediaserver.getAccessRight(self.id).then(function(result){
                    //    var accessRights = result.data;
                    //    processUsers(usersList,userGroups,accessRights);
                    //},errorHandler);
                },errorHandler);
            },errorHandler);

            return deferred.promise;
        }

        system.prototype.getUsers = function(system){
            if(!this.usersPromise){
                var self = this;
                var promise = null;
                if(self.isOnline){ // Two separate cases - either we get info from the system (presuming it has actual names)
                    promise = self.getUsersDataFromTheSystem(self.id).catch(function(){
                        return self.getUsersCachedInCloud(self.id);
                    });
                }else{ // or we get old cached data from the cloud
                    promise = self.getUsersCachedInCloud(self.id);
                }
                
                this.usersPromise = promise.then(function(users){
                    // Sort users here
                    self.users = _.sortBy(users,function(user){
                        var isMe = user.accountEmail === self.currentUserEmail;
                        var isOwner = user.accountEmail === self.info.ownerAccountEmail || user.isAdmin;
                        var isAdmin = findAccessRole(user.isAdmin, user.permissions || user.accessRole).readOnly;

                        user.canBeDeleted = !isOwner && (!isAdmin || self.isMine);
                        user.canBeEdited = !isOwner && !isMe && (!isAdmin || self.isMine);

                        return -Config.accessRoles.order.indexOf(user.accessRole);
                    });
                    // If system is reported to be online - try to get actual users list

                    return self.users;
                });

            }
            return this.usersPromise;
        };


        system.prototype.saveUser = function(user, role){
            var accessRole = role.accessRole;

            if(!user.userId){
                if(user.accountEmail == this.currentUserEmail){
                    var deferred = $q.defer();
                    deferred.reject({resultCode:'cantEditYourself'});
                    return deferred.promise;
                }

                user = _.find(this.users,function(u){
                    return user.accountEmail == u.email;
                });
                if(!user){ // user not found - create a new one
                    user = mediaserver.userObject(user.fullName, user.accountEmail);
                    self.users.push(user);
                }

                if(!user.canBeEdited && !this.isMine){
                    var deferred = $q.defer();
                    deferred.reject({resultCode:'cantEditAdmin'});
                    return deferred.promise;
                }
            }

            user.groupId = role.groupId || '';
            user.permissions = role.permissions || '';
            cloudApi.share(this.id, user.email, accessRole);

            return mediaserver.saveUser(this.id, user).then(function(result){
                user.accessRole = accessRole;
            });
        }

        system.prototype.deleteUser = function(user){
            var self = this;

            // TODO: remove later
            cloudApi.unshare(self.id, user.accountEmail);

            return mediaserver.deleteUser(self.id, user.id).then(function(){
                self.users = _.without(self.users, user);
            });
        }

        system.prototype.deleteFromCurrentAccount = function(){
            var self = this;

            if(self.currentUserRecord && self.isAvailable){
                mediaserver.deleteUser(self.id, self.currentUserRecord.id); // Try to remove me from the system directly
            }
            return cloudApi.unshare(self.id, self.currentUserEmail).then(function(){
                delete systems[self.id]
            }); // Anyway - send another request to cloud_db to remove myself
        }

        return function(systemId, email){
            if(!systems[systemId]){
                systems[systemId] = new system(systemId, email);
            }
            return systems[systemId];
        };
    }]);
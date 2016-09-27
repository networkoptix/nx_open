'use strict';

// Special service for operating with systems at high-level

angular.module('cloudApp')
    .factory('system', ['cloudApi', 'mediaserver', '$q', 'uuid2', '$log', function (cloudApi, mediaserver, $q, uuid2, $log) {

        var systems = {};

        function system(systemId,currentUserEmail){
            this.id = systemId;
            this.users = [];
            this.isAvailable = true;
            this.isOnline = false;
            this.isMine = false;
            this.groups = [];
            this.info = {name:''};
            this.permissions = {};
            this.accessRole = '';
            
            this.currentUserEmail = currentUserEmail;
            this.mediaserver = mediaserver(systemId);
            this.updateSystemState();
        }

        system.prototype.updateSystemAuth = function(){
            var self = this;
            return cloudApi.getSystemNonce(self.id).then(function(data){
                return self.mediaserver.login(data.data.nonce);
            })
        };
        system.prototype.updateSystemState = function(){
            this.stateMessage = '';
            if(!this.isAvailable){
                this.stateMessage = L.system.unavailable;
            }
            if(!this.isOnline){
                this.stateMessage = L.system.offline;
            }
        };
        system.prototype.checkPermissions = function(){
            var self = this;
            self.permissions = {};
            self.accessRole = self.info.accessRole;
            if(self.currentUserRecord){
                var role = self.findAccessRole(self.currentUserRecord);
                self.accessRole = role.name;
                self.permissions.editAdmins = self.isOwner(self.currentUserRecord);
                self.permissions.isAdmin = self.isOwner(self.currentUserRecord) || self.isAdmin(self.currentUserRecord);
                self.permissions.editUsers = self.permissions.isAdmin || self.currentUserRecord.permissions.indexOf(Config.accessRoles.editUserPermissionFlag)>=0;
            }else{
                self.accessRole = self.info.accessRole;
                if(self.isMine){
                    self.permissions.editUsers = true;
                    self.permissions.editAdmins = true;
                    self.permissions.isAdmin = true;
                }else{
                    self.permissions.editUsers = self.info.accessRole.indexOf(Config.accessRoles.editUserAccessRoleFlag)>=0;
                    self.permissions.isAdmin = self.info.accessRole.indexOf(Config.accessRoles.globalAdminAccessRoleFlag)>=0;
                }
            }
        };
        system.prototype.getInfoAndPermissions = function(){
            var self = this;
            return cloudApi.system(self.id).then(function(result){
                var error = false
                if (error = cloudApi.checkResponseHasError(result)){
                    return $q.reject(error);
                }

                if(self.info){
                    $.extend(true, self.info, result.data[0]); // Update
                }else{
                    self.info = result.data[0];
                }

                self.isOnline = self.info.stateOfHealth == Config.systemStatuses.onlineStatus;
                self.isMine = self.info.ownerAccountEmail == self.currentUserEmail;

                self.checkPermissions();

                return self.info;
            });
        };
        system.prototype.getInfo = function(){
            if(!this.infoPromise){
                var deferred = $q.defer();
                var self = this;

                this.infoPromise = $q.all([
                    this.updateSystemAuth(),
                    this.getInfoAndPermissions()
                ]);
            }
            return this.infoPromise;
        };

        system.prototype.getUsersCachedInCloud = function(){
            this.isAvailable = false;
            this.updateSystemState();
            return cloudApi.users(this.id).then(function(result){
                return result.data;
            })
        };


        function normalizePermissionString(permissions){
            return permissions.split('|').sort().join('|');
        }
        _.each(Config.accessRoles.options,function(option){
            if(option.permissions){
                option.permissions = normalizePermissionString(option.permissions);
            }
        });

        system.prototype.isEmptyGuid = function(guid){
            if(!guid){
                return true;
            }
            guid = guid.replace(/[{}0\-]/gi,'');
            return guid == '';
        };


        system.prototype.isOwner = function(user){
            return user.isAdmin || user.accountEmail === this.info.ownerAccountEmail;
        };

        system.prototype.isAdmin = function(user){
            return user.permissions && user.permissions.indexOf(Config.accessRoles.globalAdminPermissionFlag)>=0;
        };

        system.prototype.updateAccessRoles = function(){
            if(!this.accessRoles){
                var groupsList = _.map(this.groups, function(group){
                    return {
                        name: group.name,
                        groupId: group.id,
                        group: group
                    };
                });
                this.accessRoles = _.union(this.predefinedRoles, groupsList);
                this.accessRoles.push(Config.accessRoles.customPermission);
            }
            return this.accessRoles;
        };
        system.prototype.findAccessRole = function(user){
            if(!user.isEnabled){
                return { name: 'Disabled' }
            }
            var self = this;
            var role = _.find(this.accessRoles,function(role){

                if(role.isOwner){ // Owner flag has top priority and overrides everything
                    return role.isOwner == user.isAdmin;
                }
                if(!self.isEmptyGuid(role.groupId)){
                    return role.groupId == user.groupId;
                }

                // Admins has second priority
                if(self.isAdmin(role)){
                    return self.isAdmin(user);
                }
                return role.permissions == user.permissions;
            });

            return role || this.accessRoles[this.accessRoles.length - 1];
        };

        system.prototype.getUsersDataFromTheSystem = function(){
            var self = this;
            function processUsers(users, groups, predefinedRoles){
                self.predefinedRoles = predefinedRoles;
                _.each(self.predefinedRoles, function(role){
                    role.permissions = normalizePermissionString(role.permissions);
                    role.isAdmin = self.isAdmin(role);
                });

                self.groups = _.sortBy(groups,function(group){return group.name;});
                self.updateAccessRoles();

                users = _.filter(users, function(user){ return user.isCloud; });
                // var accessRightsAssoc = _.indexBy(accessRights,'userId');

                _.each(users,function(user){
                    user.permissions = normalizePermissionString(user.permissions);
                    user.role = self.findAccessRole(user);
                    user.accessRole = user.role.name;

                    user.accountEmail = user.email;

                    if(user.accountEmail == self.currentUserEmail){
                        self.currentUserRecord = user;
                        self.checkPermissions();
                    }
                });

                return users;
            }

            return self.mediaserver.getAggregatedUsersData().then(function(result){
                var usersList = result.data.reply['ec2/getUsers'];
                var userGroups = result.data.reply['ec2/getUserGroups'];
                var predefinedRoles = result.data.reply['ec2/getPredefinedRoles'];
                self.isAvailable = true;
                self.updateSystemState();
                return processUsers(usersList, userGroups, predefinedRoles)
            });
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
                    if(!$.isArray(users)){
                        return false;
                    }
                    // Sort users here
                    self.users = _.sortBy(users,function(user){
                        var isMe = user.accountEmail === self.currentUserEmail;
                        var isOwner = self.isOwner(user);
                        var isAdmin = self.isAdmin(user);

                        if(user.accountFullName && !user.fullName){
                            user.fullName = user.accountFullName;
                        }
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
            var accessRole = role.name || role.label;

            if(!user.userId){
                if(user.accountEmail == this.currentUserEmail){
                    var deferred = $q.defer();
                    deferred.reject({resultCode:'cantEditYourself'});
                    return deferred.promise;
                }

                var existingUser = _.find(this.users,function(u){
                    return user.accountEmail == u.email;
                });
                if(!existingUser){ // user not found - create a new one
                    existingUser = this.mediaserver.userObject(user.fullName, user.accountEmail);
                    this.users.push(existingUser);
                }
                user = existingUser;

                if(!user.canBeEdited && !this.isMine){
                    var deferred = $q.defer();
                    deferred.reject({resultCode:'cantEditAdmin'});
                    return deferred.promise;
                }
            }

            user.groupId = role.groupId || '';
            user.permissions = role.permissions || '';

            // TODO: remove later
            //cloudApi.share(this.id, user.email, accessRole);

            return this.mediaserver.saveUser(user).then(function(result){
                user.accessRole = accessRole;
            });
        }

        system.prototype.deleteUser = function(user){
            var self = this;

            // TODO: remove later
            //cloudApi.unshare(self.id, user.accountEmail);

            return this.mediaserver.deleteUser(user.id).then(function(){
                self.users = _.without(self.users, user);
            });
        }

        system.prototype.deleteFromCurrentAccount = function(){
            var self = this;

            if(self.currentUserRecord && self.isAvailable){
                this.mediaserver.deleteUser(self.currentUserRecord.id); // Try to remove me from the system directly
            }
            return cloudApi.unshare(self.id, self.currentUserEmail).then(function(){
                delete systems[self.id]
            }); // Anyway - send another request to cloud_db to remove myself
        }

        system.prototype.update = function(){
            var self = this;
            self.infoPromise = null; //Clear cache
            return self.getInfo().then(function(){
                if(self.usersPromise){
                    self.usersPromise = null;
                    return self.getUsers().then(function(){
                        return self;
                    });
                }
                return self;
            });
        }

        return function(systemId, email){
            if(!systems[systemId]){
                systems[systemId] = new system(systemId, email);
            }
            return systems[systemId];
        };
    }]);
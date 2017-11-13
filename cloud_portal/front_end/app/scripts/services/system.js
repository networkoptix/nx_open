'use strict';

// Special service for operating with systems at high-level

angular.module('cloudApp')
    .factory('system', ['cloudApi', 'systemAPI', '$q', 'uuid2', '$log', 'systemsProvider',
    function (cloudApi, systemAPI, $q, uuid2, $log, systemsProvider) {

        var systems = {};

        function system(systemId,currentUserEmail){
            this.id = systemId;
            this.users = [];
            this.isAvailable = true;
            this.isOnline = false;
            this.isMine = false;
            this.userRoles = [];
            this.info = {name:''};
            this.permissions = {};
            this.accessRole = '';
            
            this.currentUserEmail = currentUserEmail;
            var self = this;
            this.mediaserver = systemAPI(currentUserEmail, systemId, null, function(){
                // Unauthorised request handler
                // Some options here:
                // * Access was revoked
                // * System was disconnected from cloud
                // * Password was changed
                // * Nonce expired

                // We try to update nonce and auth on the server again
                // Other cases are not distinguishable
                return self.updateSystemAuth(true);
            });
            this.updateSystemState();
        }
        system.prototype.updateSystemAuth = function(force){
            var self = this;
            if(!force && self.auth){ //no need to update
                return $q.resolve(true);
            }
            self.auth = false;
            return cloudApi.getSystemAuth(self.id).then(function(data){
                self.auth = true;
                return self.mediaserver.setAuthKeys(data.data.authGet, data.data.authPost, data.data.authPlay);
            });
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
        system.prototype.checkPermissions = function(offline){
            var self = this;
            self.permissions = {};
            self.accessRole = self.info.accessRole;
            if(self.currentUserRecord){
                if(!offline){
                    var role = self.findAccessRole(self.currentUserRecord);
                    self.accessRole = role.name;
                }
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
            return systemsProvider.getSystem(self.id).then(function(result){
                var error = false;
                if (error = cloudApi.checkResponseHasError(result)){
                    return $q.reject(error);
                }

                if(!result.data.length){
                    return $q.reject({data:{resultCode: 'forbidden'}});
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
        system.prototype.getInfo = function(force){
            if(force){
                this.infoPromise = null;
            }
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
            var self = this;
            return cloudApi.users(this.id).then(function(result){
                 _.each(result.data,function(user){
                    user.permissions = normalizePermissionString(user.customPermissions);
                    user.email = user.accountEmail;
                    if(user.email == self.currentUserEmail){
                        self.currentUserRecord = user;
                        self.checkPermissions(true);
                    }
                });
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
            return user.isAdmin || user.email === this.info.ownerAccountEmail;
        };

        system.prototype.isAdmin = function(user){
            return user.permissions && user.permissions.indexOf(Config.accessRoles.globalAdminPermissionFlag)>=0;
        };

        system.prototype.updateAccessRoles = function(){
            if(!this.accessRoles){
                var userRolesList = _.map(this.userRoles, function(userRole){
                    return {
                        name: userRole.name,
                        userRoleId: userRole.id,
                        userRole: userRole
                    };
                });
                this.accessRoles = _.union(this.predefinedRoles, userRolesList);
                this.accessRoles.push(Config.accessRoles.customPermission);
            }
            return this.accessRoles;
        };
        system.prototype.findAccessRole = function(user){
            if(!user.isEnabled){
                return { name: 'Disabled' }
            }
            var roles = this.accessRoles || Config.accessRoles.predefinedRoles;
            var self = this;
            var role = _.find(roles,function(role){

                if(role.isOwner){ // Owner flag has top priority and overrides everything
                    return role.isOwner == user.isAdmin;
                }
                if(!self.isEmptyGuid(role.userRoleId)){
                    return role.userRoleId == user.userRoleId;
                }

                // Admins has second priority
                if(self.isAdmin(role)){
                    return self.isAdmin(user);
                }
                return role.permissions == user.permissions;
            });

            return role || roles[roles.length - 1];
        };

        system.prototype.getUsersDataFromTheSystem = function(){
            var self = this;
            function processUsers(users, userRoles, predefinedRoles){
                self.predefinedRoles = predefinedRoles;
                _.each(self.predefinedRoles, function(role){
                    role.permissions = normalizePermissionString(role.permissions);
                    role.isAdmin = self.isAdmin(role);
                });

                self.userRoles = _.sortBy(userRoles,function(userRole){return userRole.name;});
                self.updateAccessRoles();

                users = _.filter(users, function(user){ return user.isCloud; });
                // var accessRightsAssoc = _.indexBy(accessRights,'userId');

                _.each(users,function(user){
                    user.permissions = normalizePermissionString(user.permissions);
                    user.role = self.findAccessRole(user);
                    user.accessRole = user.role.name;

                    if(user.email == self.currentUserEmail){
                        self.currentUserRecord = user;
                        self.checkPermissions();
                    }
                });

                return users;
            }

            return self.mediaserver.getAggregatedUsersData().then(function(result){
                if(!result.data.reply){
                    $log.error('Aggregated request to server has failed', result);
                    return $q.reject();
                }
                var usersList = result.data.reply['ec2/getUsers'];
                var userRoles = result.data.reply['ec2/getUserRoles'];
                var predefinedRoles = result.data.reply['ec2/getPredefinedRoles'];
                self.isAvailable = true;
                self.updateSystemState();
                return processUsers(usersList, userRoles, predefinedRoles)
            });
        }

        system.prototype.getUsers = function(reload){
            if(!this.usersPromise || reload){
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
                        var isMe = user.email === self.currentUserEmail;
                        var isOwner = self.isOwner(user);
                        var isAdmin = self.isAdmin(user);

                        if(user.accountFullName && !user.fullName){
                            user.fullName = user.accountFullName;
                        }
                        user.canBeDeleted = !isOwner && (!isAdmin || self.isMine);
                        user.canBeEdited = !isOwner && !isMe && (!isAdmin || self.isMine) && user.isEnabled;

                        return -Config.accessRoles.order.indexOf(user.accessRole);
                    });
                    // If system is reported to be online - try to get actual users list

                    return self.users;
                });

            }
            return this.usersPromise;
        };


        system.prototype.saveUser = function(user, role){
            user.email = user.email.toLowerCase();
            var accessRole = role.name || role.label;

            if(!user.userId){
                if(user.email == this.currentUserEmail){
                    var deferred = $q.defer();
                    deferred.reject({resultCode:'cantEditYourself'});
                    return deferred.promise;
                }

                var existingUser = _.find(this.users,function(u){
                    return user.email == u.email;
                });
                if(!existingUser){ // user not found - create a new one
                    existingUser = this.mediaserver.userObject(user.fullName, user.email);
                    this.users.push(existingUser);
                }
                user = existingUser;

                if(!user.canBeEdited && !this.isMine){
                    var deferred = $q.defer();
                    deferred.reject({resultCode:'cantEditAdmin'});
                    return deferred.promise;
                }
            }

            user.userRoleId = role.userRoleId || '';
            user.permissions = role.permissions || '';

            // TODO: remove later
            //cloudApi.share(this.id, user.email, accessRole);

            return this.mediaserver.saveUser(user).then(function(result){
                user.role = role;
                user.accessRole = accessRole;
            });
        }

        system.prototype.deleteUser = function(user){
            var self = this;

            // TODO: remove later
            //cloudApi.unshare(self.id, user.email);

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

        system.prototype.update = function(secondAttempt){
            var self = this;
            self.infoPromise = null; //Clear cache
            return self.getInfo().then(function(){
                if(self.usersPromise){
                    self.usersPromise = null;
                    if(self.permissions.editUsers){
                        return self.getUsers().then(function(){
                            return self;
                        });
                    }else{
                        return self;
                    }
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
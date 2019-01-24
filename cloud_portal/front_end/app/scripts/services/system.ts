import * as _ from 'underscore';
import * as angular from 'angular';

(function() {
    'use strict';

// Special service for operating with systems at high-level

    angular.module('cloudApp')
            .factory('system', ['cloudApi', 'systemAPI', '$q', 'uuid2', '$log', 'systemsProvider', 'configService', 'languageService',
                function (cloudApi, systemAPI, $q, uuid2, $log, systemsProvider, configService, languageService) {

                    let systems = {};
                    const CONFIG = configService.config;
                    let L = languageService.lang;

                    function System(systemId, currentUserEmail) {
                        this.id = systemId;
                        this.users = [];
                        this.isAvailable = true;
                        this.isOnline = false;
                        this.isMine = false;
                        this.userRoles = [];
                        this.info = { name: '' };
                        this.permissions = {};
                        this.accessRole = '';
                        this.mergeInfo = {};

                        this.currentUserEmail = currentUserEmail;
                        this.mediaserver = systemAPI(currentUserEmail, systemId, null, () => {
                            // Unauthorised request handler
                            // Some options here:
                            // * Access was revoked
                            // * System was disconnected from cloud
                            // * Password was changed
                            // * Nonce expired

                            // We try to update nonce and auth on the server again
                            // Other cases are not distinguishable
                            return this.updateSystemAuth(true);
                        });
                        this.updateSystemState();
                    }

                    System.prototype.updateSystemAuth = function (force) {
                        if (!force && this.auth) { //no need to update
                            return $q.resolve(true);
                        }
                        this.auth = false;
                        return cloudApi.getSystemAuth(this.id).then((data) => {
                            this.auth = true;
                            return this.mediaserver.setAuthKeys(data.data.authGet, data.data.authPost, data.data.authPlay);
                        });
                    };

                    System.prototype.updateSystemState = function () {
                        this.stateMessage = '';
                        if (!this.isAvailable) {
                            this.stateMessage = L.system.unavailable;
                        }
                        if (!this.isOnline) {
                            this.stateMessage = L.system.offline;
                        }
                    };

                    System.prototype.checkPermissions = function (offline) {
                        this.permissions = {};
                        this.accessRole = this.info.accessRole;
                        if (this.currentUserRecord) {
                            if (!offline) {
                                let role = this.findAccessRole(this.currentUserRecord);
                                this.accessRole = role.name;
                            }
                            this.permissions.editAdmins = this.isOwner(this.currentUserRecord);
                            this.permissions.isAdmin = this.isOwner(this.currentUserRecord) || this.isAdmin(this.currentUserRecord);
                            this.permissions.editUsers = this.permissions.isAdmin || this.currentUserRecord.permissions.indexOf(CONFIG.accessRoles.editUserPermissionFlag) >= 0;
                        } else {
                            this.accessRole = this.info.accessRole;
                            if (this.isMine) {
                                this.permissions.editUsers = true;
                                this.permissions.editAdmins = true;
                                this.permissions.isAdmin = true;
                            } else {
                                this.permissions.editUsers = this.info.accessRole.indexOf(CONFIG.accessRoles.editUserAccessRoleFlag) >= 0;
                                this.permissions.isAdmin = this.info.accessRole.indexOf(CONFIG.accessRoles.globalAdminAccessRoleFlag) >= 0;
                            }
                        }
                    };

                    System.prototype.getInfoAndPermissions = function () {
                        return systemsProvider.getSystem(this.id).then((result) => {
                            let error = false;
                            if (error = cloudApi.checkResponseHasError(result)) {
                                return $q.reject(error);
                            }

                            if (!result.data.length) {
                                return $q.reject({ data: { resultCode: 'forbidden' } });
                            }
                            if (this.info) {
                                _.extend(this.info, result.data[0]); // Update
                            } else {
                                this.info = result.data[0];
                            }

                            this.isOnline = this.info.stateOfHealth == CONFIG.systemStatuses.onlineStatus;
                            this.isMine = this.info.ownerAccountEmail == this.currentUserEmail;
                            this.canMerge = this.isMine && (this.info.capabilities && this.info.capabilities.indexOf(CONFIG.systemCapabilities.cloudMerge) > -1
                                    || CONFIG.allowDebugMode
                                    || CONFIG.allowBetaMode);
                            this.mergeInfo = result.data[0].mergeInfo;

                            this.checkPermissions();

                            return this.info;
                        });
                    };

                    System.prototype.getInfo = function (force) {
                        if (force) {
                            this.infoPromise = null;
                        }
                        if (!this.infoPromise) {
                            this.infoPromise = $q.all([
                                this.updateSystemAuth(),
                                this.getInfoAndPermissions()
                            ]);
                        }
                        return this.infoPromise;
                    };

                    System.prototype.getUsersCachedInCloud = function () {
                        this.isAvailable = false;
                        this.updateSystemState();
                        return cloudApi.users(this.id).then((result) => {
                            _.each(result.data, (user) => {
                                user.permissions = normalizePermissionString(user.customPermissions);
                                user.email = user.accountEmail;
                                if (user.email == this.currentUserEmail) {
                                    this.currentUserRecord = user;
                                    this.checkPermissions(true);
                                }
                            });
                            return result.data;
                        })
                    };

                    function normalizePermissionString(permissions) {
                        return permissions.split('|').sort().join('|');
                    }

                    _.each(CONFIG.accessRoles.options, function (option) {
                        if (option.permissions) {
                            option.permissions = normalizePermissionString(option.permissions);
                        }
                    });

                    System.prototype.isEmptyGuid = function (guid) {
                        if (!guid) {
                            return true;
                        }
                        guid = guid.replace(/[{}0\-]/gi, '');
                        return guid == '';
                    };

                    System.prototype.isOwner = function (user) {
                        return user.isAdmin || user.email === this.info.ownerAccountEmail;
                    };

                    System.prototype.isAdmin = function (user) {
                        return user.permissions && user.permissions.indexOf(CONFIG.accessRoles.globalAdminPermissionFlag) >= 0;
                    };

                    System.prototype.updateAccessRoles = function () {
                        if (!this.accessRoles) {
                            let userRolesList = _.map(this.userRoles, (userRole) => {
                                return {
                                    name: userRole.name,
                                    userRoleId: userRole.id,
                                    userRole: userRole
                                };
                            });
                            this.accessRoles = _.union(this.predefinedRoles, userRolesList);
                            this.accessRoles.push(CONFIG.accessRoles.customPermission);
                        }
                        return this.accessRoles;
                    };

                    System.prototype.findAccessRole = function (user) {
                        if (!user.isEnabled) {
                            return { name: 'Disabled' }
                        }
                        let roles = this.accessRoles || CONFIG.accessRoles.predefinedRoles;
                        let role = _.find(roles, (role) => {

                            if (role.isOwner) { // Owner flag has top priority and overrides everything
                                return role.isOwner == user.isAdmin;
                            }
                            if (!this.isEmptyGuid(role.userRoleId)) {
                                return role.userRoleId == user.userRoleId;
                            }

                            // Admins has second priority
                            if (this.isAdmin(role)) {
                                return this.isAdmin(user);
                            }
                            return role.permissions == user.permissions;
                        });

                        return role || roles[roles.length - 1];
                    };

                    System.prototype.getUsersDataFromTheSystem = function () {
                        const processUsers = (users, userRoles, predefinedRoles) => {
                            this.predefinedRoles = predefinedRoles;
                            _.each(this.predefinedRoles, (role) => {
                                role.permissions = normalizePermissionString(role.permissions);
                                role.isAdmin = this.isAdmin(role);
                            });

                            this.userRoles = _.sortBy(userRoles, (userRole) => {
                                return userRole.name;
                            });
                            this.updateAccessRoles();

                            users = _.filter(users, (user) => {
                                return user.isCloud;
                            });
                            // let accessRightsAssoc = _.indexBy(accessRights,'userId');

                            _.each(users, (user) => {
                                user.permissions = normalizePermissionString(user.permissions);
                                user.role = this.findAccessRole(user);
                                user.accessRole = user.role.name;

                                if (user.email == this.currentUserEmail) {
                                    this.currentUserRecord = user;
                                    this.checkPermissions();
                                }
                            });

                            return users;
                        };

                        return this.mediaserver.getAggregatedUsersData().then((result) => {
                            if (!result.data.reply) {
                                $log.error('Aggregated request to server has failed', result);
                                return $q.reject();
                            }
                            let usersList = result.data.reply['ec2/getUsers'];
                            let userRoles = result.data.reply['ec2/getUserRoles'];
                            let predefinedRoles = result.data.reply['ec2/getPredefinedRoles'];
                            this.isAvailable = true;
                            this.updateSystemState();
                            return processUsers(usersList, userRoles, predefinedRoles)
                        });
                    };

                    System.prototype.getUsers = function (reload) {
                        if (!this.usersPromise || reload) {
                            let promise = null;
                            if (this.isOnline) { // Two separate cases - either we get info from the system (presuming it has actual names)
                                promise = this.getUsersDataFromTheSystem(this.id).catch(() => {
                                    return this.getUsersCachedInCloud(this.id);
                                });
                            } else { // or we get old cached data from the cloud
                                promise = this.getUsersCachedInCloud(this.id);
                            }

                            this.usersPromise = promise.then((users) => {
                                if (!_.isArray(users)) {
                                    return false;
                                }
                                // Sort users here
                                this.users = _.sortBy(users, (user) => {
                                    let isMe = user.email === this.currentUserEmail;
                                    let isOwner = this.isOwner(user);
                                    let isAdmin = this.isAdmin(user);

                                    if (user.accountFullName && !user.fullName) {
                                        user.fullName = user.accountFullName;
                                    }
                                    user.canBeDeleted = !isOwner && (!isAdmin || this.isMine);
                                    user.canBeEdited = !isOwner && !isMe && (!isAdmin || this.isMine) && user.isEnabled;

                                    return -CONFIG.accessRoles.order.indexOf(user.accessRole);
                                });
                                // If system is reported to be online - try to get actual users list

                                return this.users;
                            });

                        }
                        return this.usersPromise;
                    };

                    System.prototype.saveUser = function (user, role) {
                        user.email = user.email.toLowerCase();
                        let accessRole = role.name || role.label;

                        if (!user.userId) {
                            if (user.email == this.currentUserEmail) {
                                let deferred = $q.defer();
                                deferred.reject({ resultCode: 'cantEditYourself' });
                                return deferred.promise;
                            }

                            let existingUser = _.find(this.users, (u) => {
                                return user.email == u.email;
                            });
                            if (!existingUser) { // user not found - create a new one
                                existingUser = this.mediaserver.userObject(user.fullName, user.email);
                                this.users.push(existingUser);
                            }
                            user = existingUser;

                            if (!user.canBeEdited && !this.isMine) {
                                let deferred = $q.defer();
                                deferred.reject({ resultCode: 'cantEditAdmin' });
                                return deferred.promise;
                            }
                        }

                        user.userRoleId = role.userRoleId || '';
                        user.permissions = role.permissions || '';

                        // TODO: remove later
                        //cloudApi.share(this.id, user.email, accessRole);

                        return this.mediaserver.saveUser(user).then((result) => {
                            user.role = role;
                            user.accessRole = accessRole;
                        });
                    };

                    System.prototype.deleteUser = function (user) {
                        return this.mediaserver.deleteUser(user.id).then(() => {
                            this.users = _.without(this.users, user);
                        });
                    };

                    System.prototype.deleteFromCurrentAccount = function () {
                        if (this.currentUserRecord && this.isAvailable) {
                            this.mediaserver.deleteUser(this.currentUserRecord.id); // Try to remove me from the system directly
                        }
                        return cloudApi.unshare(this.id, this.currentUserEmail).then(() => {
                            delete systems[this.id]
                        }); // Anyway - send another request to cloud_db to remove mythis
                    };

                    System.prototype.update = function () {
                        this.infoPromise = null; //Clear cache
                        return this.getInfo().then(() => {
                            if (this.usersPromise) {
                                this.usersPromise = null;
                                if (this.permissions.editUsers) {
                                    return this.getUsers().then(() => {
                                        return this;
                                    });
                                } else {
                                    return this;
                                }
                            }
                            return this;
                        });
                    };

                    return (systemId, email) => {
                        if (!systems[systemId]) {
                            systems[systemId] = new System(systemId, email);
                        }
                        return systems[systemId];
                    };
                }]);
})();

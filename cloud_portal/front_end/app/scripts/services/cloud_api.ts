import * as angular from 'angular';

(function () {

    'use strict';

    angular
            .module('cloudApp.services')
            .factory('cloudApi', CloudApi);

    CloudApi.$inject = ['$http', '$q', 'nxConfigService'];

    function CloudApi($http, $q, nxConfigService) {

        const CONFIG = nxConfigService.getConfig();

        const apiBase = CONFIG.apiBase;

        let cachedResults = {};
        let cacheReceived = {};


        function cacheGet(url, cacheForever?) {
            cacheReceived[url] = 0;

            return function (command) {
                const fromCache = command == 'fromCache';
                const forceReload = command == 'reload';
                const clearCache = command == 'clearCache';

                let defer = $q.defer();
                let now = (new Date()).getTime();

                //Check for cache availability
                const haveValueInCache = typeof(cachedResults[url]) !== 'undefined';
                let cacheMiss = !haveValueInCache || // clear cache miss
                        forceReload || // ignore cache
                        !cacheForever && ((now - cacheReceived[url]) > CONFIG.cacheTimeout); // outdated cache

                if (clearCache) {
                    delete(cachedResults[url]);
                    cacheReceived[url] = 0;
                    defer.resolve(null);
                } else if (fromCache || !cacheMiss) {
                    if (haveValueInCache) {
                        defer.resolve(cachedResults[url]);
                    } else {
                        defer.reject(cachedResults[url]);
                    }
                } else {
                    $http.get(url).then(function (response) {
                        cachedResults[url] = response;
                        cacheReceived[url] = now;
                        defer.resolve(cachedResults[url]);
                    }, function (error) {
                        delete(cachedResults[url]);
                        cacheReceived[url] = 0;
                        defer.reject(error);
                    });
                }

                return defer.promise;
            }
        }

        function clearCache() {
            cachedResults = {};
            cacheReceived = {};
        }

        return {
            checkResponseHasError: function (data) {
                if (data && data.data && data.data.resultCode && data.data.resultCode != CONFIG.responseOk) {
                    return data;
                }
                return false;
            },
            account: cacheGet(apiBase + '/account'),
            login: function (email, password, remember) {
                clearCache();
                return $http.post(apiBase + '/account/login', {
                    email: email,
                    password: password,
                    remember: remember,
                    timezone: Intl && Intl.DateTimeFormat().resolvedOptions().timeZone || ""
                });
            },
            logout: function () {
                clearCache();
                return $http.post(apiBase + '/account/logout');
            },
            register: function (email, password, firstName, lastName, subscribe, code) {
                return $http.post(apiBase + '/account/register', {
                    email: email,
                    password: password,
                    first_name: firstName,
                    last_name: lastName,
                    subscribe: subscribe,
                    code: code
                });
            },

            notification_send: function (userEmail, type, message) {
                return $http.post(apiBase.replace('/api', '/notifications') + '/send', {
                    user_email: userEmail,
                    type: type,
                    message: message
                });
            },

            accountPost: function (account) {
                return $http.post(apiBase + '/account', account);
            },
            changePassword: function (newPassword, oldPassword) {
                return $http.post(apiBase + '/account/changePassword', {
                    new_password: newPassword,
                    old_password: oldPassword
                });
            },
            restorePasswordRequest: function (userEmail) {
                return $http.post(apiBase + '/account/restorePassword', {
                    user_email: userEmail
                });
            },
            restorePassword: function (code, newPassword) {
                return $http.post(apiBase + '/account/restorePassword', {
                    code: code,
                    new_password: newPassword
                });
            },
            reactivate: function (userEmail) {
                return $http.post(apiBase + '/account/activate', {
                    user_email: userEmail
                });
            },
            activate: function (code) {
                return $http.post(apiBase + '/account/activate', {
                    code: code
                });
            },
            checkCode: function (code) {
                return $http.post(apiBase + '/account/checkCode', {
                    code: code
                });
            },
            authKey: function () {
                return $http.post(apiBase + '/account/authKey');
            },
            systems: function (systemId) {
                if (systemId) {
                    return $http.get(apiBase + '/systems/' + systemId);
                }
                return $http.get(apiBase + '/systems');
            },
            renameSystem: function (systemId, systemName) {
                var self = this;
                return $http.post(apiBase + '/systems/' + systemId + '/name', {
                    name: systemName
                }).then(function (result) {
                    self.systems('clearCache');
                    return result;
                });
            },
            getSystemAuth: function (systemId) {
                return $http.get(apiBase + '/systems/' + systemId + '/auth');
            },
            getLanguages: cacheGet('/static/languages.json', true),
            changeLanguage: function (language) {
                return $http.post(apiBase + '/utils/language/', {
                    language: language
                });
            },
            getDownloads: function () {
                return $http.get(apiBase + '/utils/downloads');
            },
            getDownloadsHistory: function (build) {
                return $http.get(apiBase + '/utils/downloads/' + (build || 'history'));
            },
            getCommonPasswords: cacheGet('/static/scripts/commonPasswordsList.json', true),
            users: function (systemId) {
                return $http.get(apiBase + '/systems/' + systemId + '/users');
            },

            // TODO Remove this method in 3.0.0
            share: function (systemId, userEmail, role) {
                return $http.post(apiBase + '/systems/' + systemId + '/users', {
                    user_email: userEmail,
                    role: role
                });
            },
            unshare: function (systemId, userEmail) {
                return $http.post(apiBase + '/systems/' + systemId + '/users', {
                    user_email: userEmail,
                    role: CONFIG.accessRoles.unshare
                });
            },

            disconnect: function (systemId, password) {
                return $http.post(apiBase + '/systems/disconnect', {
                    system_id: systemId,
                    password: password
                });
            },
            connect: function (name) {
                return $http.post(apiBase + '/systems/connect', {
                    name: name
                });
            },
            merge: function (masterSystemId, slaveSystemId) {
                return $http.post(apiBase + '/systems/merge', {
                    master_system_id: masterSystemId,
                    slave_system_id: slaveSystemId
                });
            },
            sendMessage: function(type, productId, message) {
                return $http.post(apiBase + '/feedback', {
                    message, productId, type
                });

            },
            accessRoles: function (systemId) {
                console.error('This method must not be used');
                return $http.get(apiBase + '/systems/' + systemId + '/accessRoles');
            },

            visitedKey: function (key) {
                return $http.get(apiBase + '/utils/visitedKey/?key=' + encodeURIComponent(key));
            }
        };
    }
})();

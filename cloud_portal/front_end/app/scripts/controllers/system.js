(function () {

    'use strict';

    angular
        .module('cloudApp')
        .controller('SystemCtrl', NxSystemCtrl);

    NxSystemCtrl.$inject = [ '$scope', 'cloudApi', '$routeParams', '$location', 'urlProtocol', 'dialogs', 'process',
        'account', '$q', 'system', '$poll', 'page', '$timeout', 'systemsProvider',
        'authorizationCheckService', 'languageService', 'configService' ];

    function NxSystemCtrl($scope, cloudApi, $routeParams, $location, urlProtocol, dialogs, process,
                          account, $q, system, $poll, page, $timeout, systemsProvider,
                          authorizationCheckService, languageService, configService) {

        var systemId = $routeParams.systemId,
            lang     = languageService.lang,
            CONFIG   = configService.config;

        $scope.debugMode = CONFIG.allowDebugMode;

        authorizationCheckService
            .requireLogin()
            .then(function (account) {
                if (account) {
                    $scope.account = account;
                    $scope.system = system(systemId, account.email);
                    $scope.gettingSystem.run();
                }
            });

        function getMergeTarget(targetSystemId) {
            return _.find(systemsProvider.systems, function (system) {
                return targetSystemId === system.id;
            });
        }

        function setMergeStatus(mergeInfo) {
            $scope.currentlyMerging = true;
            $scope.isMaster = mergeInfo.role ? mergeInfo.role !== CONFIG.systemStatuses.slave : mergeInfo.masterSystemId === $scope.system.id;
            $scope.mergeTargetSystem = getMergeTarget(mergeInfo.anotherSystemId);
        }

        // Retrieve system info
        $scope.gettingSystem = process.init(function () {
            return $scope.system.getInfo(true); // Force reload system info when opening page
        }, {
            errorCodes : {
                forbidden: function (error) {
                    // Special handling for not having an access to the system
                    $scope.systemNoAccess = true;
                    return false;
                },
                notFound : function (error) {
                    // Special handling for not having an access to the system
                    $scope.systemNoAccess = true;
                    return false;
                },
            },
            errorPrefix: lang.errorCodes.cantGetSystemInfoPrefix
        }).then(function () {
            $scope.canMerge = $scope.system.canMerge && $scope.system.isOnline;
            if ($scope.system.mergeInfo) {
                setMergeStatus($scope.system.mergeInfo);
            }
            $scope.systemNoAccess = false;
            loadUsers();
            if ($scope.system.permissions.editUsers) {
                $scope.gettingSystemUsers.run();
            } else {
                delayedUpdateSystemInfo();
            }
        });


        var pollingSystemUpdate = null;

        function delayedUpdateSystemInfo() {
            pollingSystemUpdate = $poll(function () {
                return $scope.system.update().catch(function (error) {
                    if (error.data.resultCode === 'forbidden' || error.data.resultCode === 'notFound') {
                        connectionLost();
                    }
                });
            }, CONFIG.updateInterval);

            $scope.$on('$destroy', function (event) {
                $poll.cancel(pollingSystemUpdate);
            });
        }

        //Retrieve users list
        $scope.gettingSystemUsers = process.init(function () {
            return $scope.system.getUsers().then(function (users) {
                if ($routeParams.callShare) {
                    $scope.share().finally(cleanUrl);
                }
            }).finally(delayedUpdateSystemInfo);
        }, {
            errorPrefix: lang.errorCodes.cantGetUsersListPrefix
        });


        function loadUsers() {
            $scope.system.getUsers(true);
        }

        function cleanUrl() {
            $location.path('/systems/' + systemId, false);
        }

        function reloadSystems() {
            systemsProvider.forceUpdateSystems();
            $location.path('/systems');
        }

        $scope.disconnect = function () {
            if ($scope.system.isMine) {
                // User is the owner. Deleting system means unbinding it and disconnecting all accounts
                // dialogs.confirm(lang.system.confirmDisconnect, lang.system.confirmDisconnectTitle, lang.system.confirmDisconnectAction, 'danger').
                dialogs
                    .disconnect(systemId)
                    .result
                    .then(function (result) {
                        if ('OK' === result) {
                            reloadSystems();
                        }
                    });
            }
        };

        $scope.delete = function () {
            if (!$scope.system.isMine) {
                // User is not owner. Deleting means he'll lose access to it
                dialogs
                    .confirm(lang.system.confirmUnshareFromMe, lang.system.confirmUnshareFromMeTitle,
                        lang.system.confirmUnshareFromMeAction, 'btn-danger',
                        lang.dialogs.cancelButton)
                    .result
                    .then(function () {
                        $scope.deletingSystem = process.init(function () {
                            return $scope.system.deleteFromCurrentAccount();
                        }, {
                            successMessage: lang.system.successDeleted.replace('{{systemName}}', $scope.system.info.name),
                            errorPrefix   : lang.errorCodes.cantUnshareWithMeSystemPrefix
                        }).then(reloadSystems);

                        $scope.deletingSystem.run();
                    });
            }
        };

        $scope.rename = function () {
            return dialogs
                .rename(systemId, $scope.system.info.name)
                .result
                .then(function (finalName) {
                    $scope.system.info.name = finalName;
                });
        };

        $scope.mergeSystems = function () {
            dialogs
                .merge($scope.system)
                .result
                .then(function (mergeInfo) {
                    setMergeStatus(mergeInfo);
                });
        };

        $scope.share = function () {
            // Call share dialog, run process inside
            return dialogs
                .share($scope.system)
                .result
                .then(loadUsers);
        };

        $scope.locked = {};
        $scope.editShare = function (user) {
            //Pass user inside

            if ($scope.locked[ user.email ]) {
                return;
            }
            $scope.locked[ user.email ] = true;
            return dialogs
                .share($scope.system, user)
                .result
                .then(loadUsers)
                .finally(function () {
                    $scope.locked[ user.email ] = false;
                });
        };

        $scope.unshare = function (user) {
            if ($scope.account.email === user.email) {
                return $scope.delete();
            }
            if ($scope.locked[ user.email ]) {
                return;
            }
            $scope.locked[ user.email ] = true;
            dialogs
                .confirm(lang.system.confirmUnshare, lang.system.confirmUnshareTitle,
                    lang.system.confirmUnshareAction, 'btn-danger',
                    lang.dialogs.cancelButton)
                .result
                .then(function (result) {
                    if ('OK' === result) {
                        // Run a process of sharing
                        $poll.cancel(pollingSystemUpdate);
                        $scope.unsharing = process.init(function () {
                            return $scope.system.deleteUser(user);
                        }, {
                            successMessage: lang.system.permissionsRemoved.replace('{{email}}', user.email),
                            errorPrefix   : lang.errorCodes.cantSharePrefix
                        }).then(function () {
                            $scope.locked[ user.email ] = false;
                            $scope.system.getUsers();
                            delayedUpdateSystemInfo();
                        }, function () {
                            $scope.locked[ user.email ] = false;
                        });
                        $scope.unsharing.run();
                    }
                }, function () {
                    $scope.locked[ user.email ] = false;
                    $scope.system.getUsers();
                    delayedUpdateSystemInfo();
                });
        };

        $scope.$watch('system.info.name', function (value) {
            page.title(value ? value + ' -' : '');
            systemsProvider.forceUpdateSystems();
        });

        function normalizePermissionString(permissions) {
            return permissions.split('|').sort().join('|');
        }

        _.each(CONFIG.accessRoles.options, function (option) {
            if (option.permissions) {
                option.permissions = normalizePermissionString(option.permissions);
            }
        });

        function connectionLost() {
            dialogs.notify(lang.errorCodes.lostConnection.replace("{{systemName}}",
                $scope.system.info.name || lang.errorCodes.thisSystem), 'warning');
            $location.path("/systems");
        }

        var cancelSubscription = $scope.$on("unauthorized_" + $routeParams.systemId, connectionLost);

        $scope.$on('$destroy', function (event) {
            cancelSubscription();
            dialogs.dismissNotifications();
        });


    }
})();

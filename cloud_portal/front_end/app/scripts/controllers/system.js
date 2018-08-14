'use strict';

angular.module('cloudApp')
    .controller('SystemCtrl', [ '$scope', 'cloudApi', '$routeParams', '$location', 'urlProtocol', 'dialogs', 'process',
        'account', 'authorizationCheckService', '$q', 'system', '$poll', 'page', '$timeout', 'systemsProvider',
        function ($scope, cloudApi, $routeParams, $location, urlProtocol, dialogs, process,
                  account, authorizationCheckService, $q, system, $poll, page, $timeout, systemsProvider) {

            var systemId = $routeParams.systemId;
            $scope.debugMode = Config.allowDebugMode;

            authorizationCheckService.requireLogin().then(function (account) {
                $scope.account = account;
                $scope.system = system(systemId, account.email);
                $scope.gettingSystem.run();

                $scope.$watch('system.info.name', function (value) {
                    page.title(value ? value + ' -' : '');
                    systemsProvider.forceUpdateSystems();
                });
            });

            function getMergeTarget(targetSystemId) {
                return _.find(systemsProvider.systems, function (system) {
                    return targetSystemId === system.id;
                });
            }

            function setMergeStatus(mergeInfo) {
                $scope.currentlyMerging = true;
                $scope.isMaster = mergeInfo.role ? mergeInfo.role !== Config.systemStatuses.slave : mergeInfo.masterSystemId === $scope.system.id;
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
                errorPrefix: L.errorCodes.cantGetSystemInfoPrefix
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
                    return $scope.system
                        .update()
                        .catch(function (error) {
                            if (error.data.resultCode === 'forbidden' || error.data.resultCode === 'notFound') {
                                connectionLost();
                            }
                        });
                }, Config.updateInterval);

                $scope.$on('$destroy', function (event) {
                    // $poll.cancel(pollingSystemUpdate);
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
                errorPrefix: L.errorCodes.cantGetUsersListPrefix
            });


            function loadUsers() {
                $scope.system.getUsers(true);
            }

            function cleanUrl() {
                $location.path('/systems/' + systemId, false);
            }

            function reloadSystems() {
                systemsProvider
                    .forceUpdateSystems()
                    .then(function () {
                        $location.path('/systems');
                    });
            }

            function updateAndGoToSystems() {
                $scope.userDisconnectSystem = true;
                systemsProvider.forceUpdateSystems().then(function () {
                    $timeout(function () {
                        $location.path('/systems')
                    })
                });
            }

            $scope.disconnect = function () {
                if ($scope.system.isMine) {
                    // User is the owner. Deleting system means unbinding it and disconnecting all accounts
                    // dialogs.confirm(L.system.confirmDisconnect, L.system.confirmDisconnectTitle, L.system.confirmDisconnectAction, 'danger').
                    dialogs.disconnect(systemId)
                        .then(function (result) {
                            if (result) {
                                updateAndGoToSystems();
                            }
                        });
                }
            };

            $scope.rename = function () {
                return dialogs
                    .rename(systemId, $scope.system.info.name)
                    .then(function (finalName) {
                        if (finalName) {
                            $scope.system.info.name = finalName;
                        }
                    });
            };

            $scope.mergeSystems = function () {
                dialogs.merge($scope.system).then(function (mergeInfo) {
                    if(mergeInfo) {
                        setMergeStatus(mergeInfo);
                    }
                });
            };

            $scope.share = function () {
                // Call share dialog, run process inside
                return dialogs
                    .share($scope.system)
                    .then(function (result) {
                        if (result) {
                            loadUsers();
                        }
                    }, function (reason) {
                        // dialog was dismissed ... this handler is required if dialog is dismissible
                        // if we don't handle it will raise a JS error
                        // ERROR Error: Uncaught (in promise): [object Number]
                    });
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
                dialogs.confirm(L.system.confirmUnshare,
                    L.system.confirmUnshareTitle,
                    L.system.confirmUnshareAction,
                    'btn-danger', 'Cancel')
                    .then(function (result) {
                        if (result) {
                            // Run a process of sharing
                            $poll.cancel(pollingSystemUpdate);
                            $scope.unsharing = process.init(function () {
                                return $scope.system.deleteUser(user);
                            }, {
                                successMessage: L.system.permissionsRemoved.replace('{{email}}', user.email),
                                errorPrefix   : L.errorCodes.cantSharePrefix
                            }).then(function () {
                                $scope.locked[ user.email ] = false;
                                $scope.system.getUsers();
                                delayedUpdateSystemInfo();
                            }, function () {
                                $scope.locked[ user.email ] = false;
                            });

                            $scope.unsharing.run();
                        } else {
                            $scope.locked[ user.email ] = false;
                            $scope.system.getUsers();
                            delayedUpdateSystemInfo();
                        }
                    }, function () {
                        $scope.locked[ user.email ] = false;
                        $scope.system.getUsers();
                        delayedUpdateSystemInfo();
                    });
            };

            _.each(Config.accessRoles.options, function (option) {
                if (option.permissions) {
                    option.permissions = normalizePermissionString(option.permissions);
                }
            });

            function normalizePermissionString(permissions) {
                return permissions.split('|').sort().join('|');
            }

            _.each(Config.accessRoles.options, function (option) {
                if (option.permissions) {
                    option.permissions = normalizePermissionString(option.permissions);
                }
            });

            function connectionLost() {
                dialogs.notify(L.errorCodes.lostConnection.replace("{{systemName}}",
                                $scope.system.info.name || L.errorCodes.thisSystem), 'warning');
                $location.path('/systems');
            }

            var cancelSubscription = $scope.$on("unauthorized_" + $routeParams.systemId, connectionLost);

    }]);

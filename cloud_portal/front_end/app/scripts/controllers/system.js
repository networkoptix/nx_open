'use strict';

angular.module('cloudApp')
    .controller('SystemCtrl', ['$scope', 'cloudApi', '$routeParams', '$location', 'urlProtocol', 'dialogs', 'process',
    'account', '$q', 'system', '$timeout',
    function ($scope, cloudApi, $routeParams, $location, urlProtocol, dialogs, process, account, $q, system, $timeout) {

        $scope.Config = Config;
        $scope.L = L;
        var systemId = $routeParams.systemId;


        account.requireLogin().then(function(account){
            $scope.account = account;
            $scope.system = system(systemId, account.email);
            $scope.gettingSystem.run();
        });

        // Retrieve system info
        $scope.gettingSystem = process.init(function(){
            return $scope.system.getInfo();
        }, {
            errorCodes: {
                forbidden: L.errorCodes.systemForbidden,
                notFound: L.errorCodes.systemNotFound
            },
            errorPrefix:'System info is unavailable:'
        }).then(loadUsers);


        var stopTimeout = false;
        function delayedUpdateSystemInfo(){
            if(!stopTimeout){
                $scope.updateTimeout = $timeout(function(){
                    return $scope.system.update().finally(delayedUpdateSystemInfo);
                },Config.updateInterval);
            }
            return $scope.updateTimeout;
        }
        $scope.$on("$destroy", function( event ) { stopTimeout = true; } );

        //Retrieve users list
        $scope.gettingSystemUsers = process.init(function(){
            return $scope.system.getUsers();
        },{
            errorPrefix:'Users list is unavailable:'
        }).then(function(users){
            if($routeParams.callShare){
                $scope.share().finally(cleanUrl);
            }
            delayedUpdateSystemInfo();
        });


        function loadUsers(){
            $scope.gettingSystemUsers.run();
        }
        function cleanUrl(){
            $location.path('/systems/' + systemId, false);
        }

        $scope.open = function(){
            urlProtocol.open(systemId);
        };

        function reloadSystems(){
            cloudApi.systems('clearCache').then(function(){
               $location.path('/systems');
            });
        }

        $scope.disconnect = function(){
            if($scope.system.isMine){
                // User is the owner. Deleting system means unbinding it and disconnecting all accounts
                // dialogs.confirm(L.system.confirmDisconnect, L.system.confirmDisconnectTitle, L.system.confirmDisconnectAction, 'danger').
                dialogs.disconnect(systemId).then(reloadSystems);
            }
        };

        $scope.delete = function(){
            if(!$scope.system.isMine){
                // User is not owner. Deleting means he'll lose access to it
                dialogs.confirm(L.system.confirmUnshareFromMe, L.system.confirmUnshareFromMeTitle, L.system.confirmUnshareFromMeAction, 'danger').
                    then(function(){
                        $scope.deletingSystem = process.init(function(){
                            return $scope.system.deleteFromCurrentAccount();
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
            return dialogs.share($scope.system).then(loadUsers);
        };

        $scope.editShare = function(user){
            //Pass user inside
            return dialogs.share($scope.system, user).then(loadUsers);
        };

        $scope.unshare = function(user){
            if($scope.account.email == user.accountEmail){
                return $scope.delete();
            }
            dialogs.confirm(L.system.confirmUnshare, L.system.confirmUnshareTitle, L.system.confirmUnshareAction, 'danger').
                then(function(){
                    // Run a process of sharing
                    $scope.unsharing = process.init(function(){
                        return $scope.system.deleteUser(user);
                    },{
                        successMessage: L.system.permissionsRemoved.replace('{accountEmail}',user.accountEmail),
                        errorPrefix:'Sharing failed:'
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


    }]);
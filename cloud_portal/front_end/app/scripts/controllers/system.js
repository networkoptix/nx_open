'use strict';

angular.module('cloudApp')
    .controller('SystemCtrl', ['$scope', 'cloudApi', '$routeParams', '$location', 'urlProtocol', 'dialogs', 'process',
    'account', '$q', 'system', '$poll', 'page', '$timeout',
    function ($scope, cloudApi, $routeParams, $location, urlProtocol, dialogs, process,
    account, $q, system, $poll, page, $timeout) {

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

        function delayedUpdateSystemInfo(){
            var pollingSystemUpdate = $poll(function(){
                return $scope.system.update();
            },Config.updateInterval);

            $scope.$on("$destroy", function( event ) {
                $poll.cancel(pollingSystemUpdate);
            } );
        }

        //Retrieve users list
        $scope.gettingSystemUsers = process.init(function(){
            return $scope.system.getUsers().then(function(users){
                if($routeParams.callShare){
                    return $scope.share().finally(cleanUrl);
                }
            }).finally(delayedUpdateSystemInfo);
        },{
            errorPrefix:'Users list is unavailable:'
        });


        function loadUsers(){
            $scope.gettingSystemUsers.run();
        }
        function cleanUrl(){
            $location.path('/systems/' + systemId, false);
        }

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

        $scope.rename = function(){
            return dialogs.rename(systemId, $scope.system.info.name).then(function(finalName){
                $scope.system.info.name = finalName;
            });
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
            if($scope.account.email == user.email){
                return $scope.delete();
            }
            dialogs.confirm(L.system.confirmUnshare, L.system.confirmUnshareTitle, L.system.confirmUnshareAction, 'danger').
                then(function(){
                    // Run a process of sharing
                    $scope.unsharing = process.init(function(){
                        return $scope.system.deleteUser(user);
                    },{
                        successMessage: L.system.permissionsRemoved.replace('{email}',user.email),
                        errorPrefix:'Sharing failed:'
                    });
                    $scope.unsharing.run();
                });
        };

        $scope.$watch('system.info.name',function(value){
            page.title(value + ' -');
        });

        function normalizePermissionString(permissions){
            return permissions.split('|').sort().join('|');
        }
        _.each(Config.accessRoles.options,function(option){
            if(option.permissions){
                option.permissions = normalizePermissionString(option.permissions);
            }
        });


    }]);
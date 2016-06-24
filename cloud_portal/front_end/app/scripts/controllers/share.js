'use strict';

angular.module('cloudApp')
    .controller('ShareCtrl', ['$scope', 'cloudApi', 'process', 'dialogs', '$q', 'account', 'mediaserver',
    function ($scope, cloudApi, process, dialogs, $q, account, mediaserver) {

        $scope.Config = Config;
        $scope.L = L;
        $scope.buttonText = L.sharing.shareConfirmButton;

        var dialogSettings = dialogs.getSettings($scope);
        var system = dialogSettings.params.system;
        var systemId = system.id;
        $scope.isNewShare = !dialogSettings.params.user;

        $scope.user = dialogSettings.params.user? _.clone(dialogSettings.params.user):{
            accountEmail:'',
            accessRole: Config.accessRoles.default
        };

        if(!$scope.isNewShare) {
            account.get().then(function (account) {
                if(account.email == $scope.user.accountEmail) {
                    $scope.$parent.cancel(L.share.cantEditYourself);
                }
            });
            $scope.buttonText = L.sharing.editShareConfirmButton;
        }

        function findRole(user, accessRoles){
            return _.find(accessRoles,function(role){
                if(!user.localData){
                    return role.accessRole == user.accessRole;
                }
                if(user.localData.groupId){
                    return role.groupId == user.localData.groupId;
                }

                return user.localData.permissions == role.permissions;
            });
        }

        function processAccessRoles(roles){
            var filteredRoles = _.filter(roles,function(role){
                role.label = L.accessRoles[role.accessRole]? L.accessRoles[role.accessRole].label : role.accessRole;
                return  (Config.accessRoles.order.indexOf(role.accessRole)>=0) &&   // 1. Access role must be registered
                        role.accessRole != Config.accessRoles.unshare;            // 2. Remove unsharing role from interface
            });

            $scope.accessRoles = _.sortBy(filteredRoles,function(role){
                var index = Config.accessRoles.order.indexOf(role.accessRole);
                return index >= 0 ? index: 10000;
            });

            if(system.cacheData){
                // Add groups sorted by name
                _.each(system.cacheData.groups,function(group){
                    $scope.accessRoles.push({
                        accessRole: Config.accessRoles.custom,
                        groupId: group.id,
                        label: group.name
                    })
                });
            }
            $scope.accessRoles.push({
                accessRole: Config.accessRoles.custom,
                label: L.accessRoles.custom.label
            })
            $scope.user.role = findRole($scope.user, $scope.accessRoles);
        }

        processAccessRoles([{ accessRole: $scope.user.accessRole }]);
        // dialogSettings.params.
        cloudApi.accessRoles(systemId).then(function(roles){
            processAccessRoles(roles.data);
        },function(){
            processAccessRoles(Config.accessRoles.options);
        });


        // TODO: add groups from system cache


        $scope.formatUserName = function(){
            var user = $scope.user;

            if(!user.fullName || user.fullName.trim() == ''){
                return user.accountEmail;
            }

            return user.fullName + ' (' + user.accountEmail +')';
        };

        function doShare(){
            var role = $scope.user.role;
            var accessRole = role.accessRole;
            var user = $scope.user;

            console.log("doShare", role, user);
            //$scope.isNewShare


            // 1. Try to apply changes to the system
            function saveLocal(){
                if(!user.localData){
                    user.localData = mediaserver.userObject(user.fullName, user.accountEmail);
                }

                user.localData.groupId = role.groupId||'';
                user.localData.permissions = role.permissions||'';

                return mediaserver.saveUser(systemId, user.localData);
            }

            var deferred = $q.defer();

            // 2. Save changes to the cloud
            function saveCloud(){
                if(role.groupId){
                    role.accessRole = Config.accessRoles.custom;
                }

                return cloudApi.share(systemId, $scope.user.accountEmail, role.accessRole).then(function(success){
                    deferred.resolve(success);
                },function(error){
                    deferred.reject(error);
                })
            }

            var mustSaveLocal = user.localData || $scope.user.role.groupId;
            saveLocal().then(function(){
                saveCloud();
            },function(error){
                if(mustSaveLocal){
                    return deferred.reject(error);
                }
                saveCloud();
            });

            return deferred.promise;
        }

        $scope.sharing = process.init(function(){
            if($scope.user.accessRole == Config.accessRoles.owner) {
                var deferred = $q.defer();

                dialogs.confirm(L.sharing.confirmOwner).then(
                    function () {
                        doShare().then(function (data) {
                            deferred.resolve(data);
                        }, function () {
                            deferred.reject(data);
                        });
                    }
                );

                return deferred.promise;
            }else{
                return doShare();
            }
        },{
            successMessage: 'New permissions saved'
        }).then(function(){
            $scope.$parent.ok($scope.user);
        });

        $scope.close = function(){
            dialogs.closeMe($scope);
        }
    }]);
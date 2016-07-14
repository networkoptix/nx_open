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
            var role = _.find(accessRoles,function(role){
                if(!system.isEmptyGuid(user.groupId)){
                    return role.groupId === user.groupId;
                }

                return user.permissions === role.permissions && !role.groupId;
            });

            return role || accessRoles[accessRoles.length-1]; // Last option is custom
        }

        function processAccessRoles(){
            var filteredRoles = _.filter(Config.accessRoles.options,function(role){
                if(role.isAdmin || role.readOnly && !system.isMine){
                    return false;
                }
                role.label = L.accessRoles[role.accessRole]? L.accessRoles[role.accessRole].label : role.accessRole;
                return  (Config.accessRoles.order.indexOf(role.accessRole)>=0) &&   // 1. Access role must be registered
                        role.accessRole != Config.accessRoles.unshare;            // 2. Remove unsharing role from interface
            });

            $scope.accessRoles = _.sortBy(filteredRoles,function(role){
                var index = Config.accessRoles.order.indexOf(role.accessRole);
                return index >= 0 ? index: 10000;
            });

            if(system.groups){
                // Add groups sorted by name
                _.each(system.groups,function(group){
                    $scope.accessRoles.push({
                        accessRole: Config.accessRoles.custom.accessRole,
                        groupId: group.id,
                        label: group.name
                    })
                });
            }
            $scope.accessRoles.push({
                accessRole: Config.accessRoles.custom.accessRole,
                label: L.accessRoles.custom.label
            })
            $scope.user.role = findRole($scope.user, $scope.accessRoles);
        }

        processAccessRoles();


        $scope.formatUserName = function(){
            var user = $scope.user;

            if(!user.fullName || user.fullName.trim() == ''){
                return user.accountEmail;
            }

            return user.fullName + ' (' + user.accountEmail +')';
        };

        function doShare(){
            return system.saveUser($scope.user, $scope.user.role);
        }

        $scope.getRoleDescription = function(){
            if($scope.user.role.groupId){
                return L.accessRoles.customRole.description;
            }
            if(L.accessRoles[$scope.user.role.accessRole]){
                return L.accessRoles[$scope.user.role.accessRole].description;
            }

            return L.accessRoles.custom.description;
        }

        $scope.sharing = process.init(function(){
            if($scope.user.role.accessRole == Config.accessRoles.owner) {
                return dialogs.confirm(L.sharing.confirmOwner).then(doShare);
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
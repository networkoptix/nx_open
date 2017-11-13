'use strict';

angular.module('cloudApp')
    .controller('ShareCtrl', ['$scope', 'cloudApi', 'process', 'dialogs', '$q', 'account',
    function ($scope, cloudApi, process, dialogs, $q, account) {

        $scope.buttonText = L.sharing.shareConfirmButton;

        var dialogSettings = dialogs.getSettings($scope);
        var system = dialogSettings.params.system;
        var systemId = system.id;
        $scope.isNewShare = !dialogSettings.params.user;

        $scope.user = dialogSettings.params.user ? _.clone(dialogSettings.params.user):{
            email:'',
            isEnabled: true
        };

        if(!$scope.isNewShare) {
            account.get().then(function (account) {
                if(account.email == $scope.user.email) {
                    $scope.$parent.cancel(L.share.cantEditYourself);
                }
            });
            $scope.buttonText = L.sharing.editShareConfirmButton;
        }

        function processAccessRoles(){
            $scope.accessRoles = _.filter(system.accessRoles || Config.accessRoles.predefinedRoles,function(role){
                return ! (role.isOwner || role.isAdmin && !system.isMine);
            });

            if(!$scope.user.role){
                $scope.user.role = system.findAccessRole($scope.user);
            }
        }

        processAccessRoles();


        $scope.formatUserName = function(){
            var user = $scope.user;

            if(!user.fullName || user.fullName.trim() == ''){
                return user.email;
            }

            return user.fullName + ' (' + user.email +')';
        };

        function doShare(){
            return system.saveUser($scope.user, $scope.user.role);
        }

        $scope.getRoleDescription = function(){
            if($scope.user.role.description){
                return $scope.user.role.description;
            }
            if($scope.user.role.userRoleId){
                return L.accessRoles.customRole.description;
            }
            if(L.accessRoles[$scope.user.role.name]){
                return L.accessRoles[$scope.user.role.name].description;
            }
            return L.accessRoles.customRole.description;
        }

        $scope.sharing = process.init(function(){
            if($scope.user.role.isOwner) {
                return dialogs.confirm(L.sharing.confirmOwner).then(doShare);
            }else{
                return doShare();
            }
        },{
            successMessage: L.sharing.permissionsSaved
        }).then(function(){
            $scope.$parent.ok($scope.user);
        });

        $scope.close = function(){
            dialogs.closeMe($scope);
        }

        $scope.$on('$destroy', function(){
            dialogs.dismissNotifications();
        });
    }]);

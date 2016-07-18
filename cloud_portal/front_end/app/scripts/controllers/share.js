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

        $scope.user = dialogSettings.params.user ? _.clone(dialogSettings.params.user):{
            accountEmail:''
        };

        if(!$scope.isNewShare) {
            account.get().then(function (account) {
                if(account.email == $scope.user.accountEmail) {
                    $scope.$parent.cancel(L.share.cantEditYourself);
                }
            });
            $scope.buttonText = L.sharing.editShareConfirmButton;
        }

        function processAccessRoles(){
            $scope.accessRoles = _.filter(system.accessRoles,function(role){
                return ! (role.isOwner || role.isAdmin && !system.isMine);
            });

            $scope.user.role = system.findAccessRole($scope.user);
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
            if($scope.user.role.description){
                return $scope.user.role.description;
            }
            if($scope.user.role.groupId){
                return L.accessRoles.customRole.description;
            }
            if(L.accessRoles[$scope.user.role.name]){
                return L.accessRoles[$scope.user.role.name].description;
            }
            return L.accessRoles.custom.description;
        }

        $scope.sharing = process.init(function(){
            if($scope.user.role.isOwner) {
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
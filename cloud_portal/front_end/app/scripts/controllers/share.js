'use strict';

angular.module('cloudApp')
    .controller('ShareCtrl', function ($scope, cloudApi, process, dialogs, $q, account) {


        // TODO: We must replace this hack with something more angular-way,
        // but I can't figure out yet, how to implement dialog service and pass parameters to controllers
        // we need something like modalInstance
        function findSettings($scope){
            return $scope.settings || $scope.$parent && findSettings($scope.$parent) || null;
        }

        $scope.Config = Config;
        $scope.L = L;
        $scope.buttonText = L.sharing.shareConfirmButton;

        var dialogSettings = findSettings($scope);

        var systemId = dialogSettings.params.systemId;

        $scope.lockEmail = !!dialogSettings.params.share;
        $scope.share = dialogSettings.params.share? _.clone(dialogSettings.params.share):{
            accountEmail:'',
            accessRole: Config.accessRoles.default
        };

        if($scope.lockEmail) {
            account.get().then(function (account) {
                if(account.email == $scope.share.accountEmail) {
                    $scope.$parent.cancel(L.share.cantEditYourself);
                }
            });
            $scope.buttonText = L.sharing.editShareConfirmButton;
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
        }

        processAccessRoles([{ accessRole: $scope.share.accessRole }]);
        cloudApi.accessRoles(systemId).then(function(roles){
            processAccessRoles(roles.data);
        },function(){
            processAccessRoles(Config.accessRoles.options);
        });

        $scope.formatUserName = function(){
            var share = $scope.share;

            if(!share.fullName || share.fullName.trim() == ''){
                return share.accountEmail;
            }

            return share.fullName + ' (' + share.accountEmail +')';
        };

        function doShare(){
            return cloudApi.share(systemId, $scope.share.accountEmail, $scope.share.accessRole);
        }
        $scope.sharing = process.init(function(){
            if($scope.share.accessRole == Config.accessRoles.owner) {
                var defered = $q.defer();

                dialogs.confirm(L.sharing.confirmOwner).then(
                    function () {
                        doShare().then(function (data) {
                            defered.resolve(data);
                        }, function () {
                            defered.reject(data);
                        });
                    }
                );

                return defered.promise;
            }else{
                return doShare();
            }
        }).then(function(){
            $scope.$parent.ok($scope.share);
        });

        $scope.close = function(){
            dialogs.closeMe($scope);
        }
    });
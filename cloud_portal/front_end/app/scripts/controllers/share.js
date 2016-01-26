'use strict';

angular.module('cloudApp')
    .controller('ShareCtrl', function ($scope, cloudApi, process, dialogs, $q) {

        // TODO: We must replace this hack with something more angular-way,
        // but I can't figure out yet, how to implement dialog service and pass parameters to controllers
        // we need something like modalInstance
        function findSettings($scope){
            return $scope.settings || $scope.$parent && findSettings($scope.$parent) || null;
        }

        var dialogSettings = findSettings($scope);

        var systemId = dialogSettings.params.systemId;
        var isOwner = dialogSettings.params.isOwner;
        $scope.lockEmail = !!dialogSettings.params.share;
        $scope.share = dialogSettings.params.share? _.clone(dialogSettings.params.share):{
            accountEmail:'',
            accessRole: Config.accessRolesSettings.default
        };

        function processAccessRoles(roles){
            roles.push({ accessRole: Config.accessRolesSettings.owner }); // TODO: remove this later. This was added for demo purposes.

            var filteredRoles = _.filter(roles,function(role){
                role.label = Config.accessRolesSettings.labels[role.accessRole] || role.accessRole;
                return  (Config.accessRolesSettings.order.indexOf(role.accessRole)>=0) &&   // 1. Access role must be registered
                        role.accessRole != Config.accessRolesSettings.unshare &&            // 2. Remove unsharing role from interface
                        (isOwner || role.accessRole != Config.accessRolesSettings.owner);   // 3. Remove owner role for not owner TODO: remove this later. This was added for demo purposes.
            });

            $scope.accessRoles = _.sortBy(filteredRoles,function(role){
                var index = Config.accessRolesSettings.order.indexOf(role.accessRole);
                return index >= 0 ? index: 10000;
            });
        }


        processAccessRoles([{ accessRole: $scope.share.accessRole }]);
        cloudApi.accessRoles(systemId).then(function(roles){
            processAccessRoles(roles.data);
        },function(){
            processAccessRoles(Config.accessRolesSettings.options);
        });

        function doShare(){
            return cloudApi.share(systemId, $scope.share.accountEmail, $scope.share.accessRole);
        }
        $scope.sharing = process.init(function(){
            if($scope.share.accessRole == Config.accessRolesSettings.owner) {
                var defered = $q.defer();

                dialogs.confirm("You are going to change the owner of your system. You will not be able to return this power back!").then(
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
    });
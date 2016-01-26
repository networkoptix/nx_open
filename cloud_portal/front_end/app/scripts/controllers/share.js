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
        $scope.share = dialogSettings.params.share || {
            accountEmail:'',
            accessRole: Config.accessRolesSettings.default
        };

        function processAccessRoles(roles){
            _.each(roles,function(role){
                role.label = Config.accessRolesSettings.labels[role.accessRole] || role.accessRole;
            });

            var filteredRoles = _.filter(roles,function(role){
                return role.accessRole != Config.accessRolesSettings.unshare && (isOwner || role.accessRole != Config.accessRolesSettings.owner);
            });

            $scope.accessRoles = _.sortBy(filteredRoles,function(role){
                var index = Config.accessRolesSettings.order.indexOf(role.accessRole);
                return index >= 0 ? index: 10000;
            });
        }
        processAccessRoles(Config.accessRolesSettings.options);

        cloudApi.accessRoles().then(function(roles){
            processAccessRoles(roles.data);
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
            // Update users list

            // Dismiss dialog
            $scope.$parent.close($scope.share);
        });
    });
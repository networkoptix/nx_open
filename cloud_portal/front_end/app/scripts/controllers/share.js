'use strict';

angular.module('cloudApp')
    .controller('ShareCtrl', function ($scope, cloudApi, process) {

        // TODO: We must replace this hack with something more angular-way,
        // but I can't figure out yet, how to implement dialog service and pass parameters to controllers
        // we need something like modalInstance
        function findSettings($scope){
            return $scope.settings || $scope.$parent && findSettings($scope.$parent) || null;
        }

        var dialogSettings = findSettings($scope);

        var systemId = dialogSettings.params.systemId;
        $scope.share = dialogSettings.params.share || {
            accountEmail:'',
            accessRole: Config.accessRolesSettings.default
        };

        function processAccessRoles(roles){
            _.each(roles,function(role){
                role.label = Config.accessRolesSettings.labels[role.accessRole] || role.accessRole;
            });

            var filteredRoles = _.filter(roles,function(role){
                return role.accessRole != Config.accessRolesSettings.unshare && role.accessRole != Config.accessRolesSettings.owner;
            });

            $scope.accessRoles = _.sortBy(filteredRoles,function(role){
                var index = Config.accessRolesSettings.order.indexOf(role);
                return index > 0 ? index: 10000;
            });
        }
        processAccessRoles(Config.accessRolesSettings.options);

        cloudApi.accessRoles().then(function(roles){
            processAccessRoles(roles.data);
        });

        $scope.sharing = process.init(function(){
            return cloudApi.share(systemId, $scope.share.accountEmail, $scope.share.accessRole);
        }).then(function(){
            // Update users list

            // Dismiss dialog
            $scope.$parent.close($scope.share);
        });
    });
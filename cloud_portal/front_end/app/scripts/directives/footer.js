'use strict';

angular.module('cloudApp')
    .directive('footer', ['account', function (account) {
        return {
            restrict: 'E',
            templateUrl: Config.viewsDir + 'components/footer.html',
            link: function (scope) {
                account.requireLogin().then(function (account) {
                    scope.account = account;
                    scope.downloadsHistory = scope.account.permissions.indexOf(Config.permission.canViewRelease) > -1;
                });

            }
        }
    }]);

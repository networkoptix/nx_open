(function () {

    'use strict';

    angular
        .module('cloudApp.directives')
        .directive('nxFooter', NxFooter);

    NxFooter.$inject = [ 'configService', 'account' ];

    function NxFooter(configService, account) {

        var CONFIG = configService.config,
            user = {};

        return {
            restrict   : 'E',
            templateUrl: CONFIG.viewsDir + 'components/footer.html',
            link       : function (scope, element, attrs) {
                scope.canViewHistory = false;

                account.get().then(function (account) {
                    user = account;
                    if (user.permissions.indexOf('can_view_release') > -1) {
                        scope.canViewHistory = true;
                    }
                });
            }
        }
    }
})();

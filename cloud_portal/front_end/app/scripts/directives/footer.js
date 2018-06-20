(function () {

    'use strict';

    angular
        .module('cloudApp.directives')
        .directive('nxFooter', NxFooter);

    NxFooter.$inject = [ '$rootScope', 'configService', 'account' ];

    function NxFooter($rootScope, configService, account) {

        var CONFIG = configService.config,
            user   = {};

        return {
            restrict   : 'E',
            templateUrl: CONFIG.viewsDir + 'components/footer.html',
            link       : function (scope, element, attrs) {
                scope.viewFooter = true;
                scope.canViewHistory = false;

                account.get().then(function (account) {
                    user = account;
                    if (user.permissions.indexOf('can_view_release') > -1) {
                        scope.canViewHistory = true;
                    }
                });

                $rootScope.$on('nx.system.online', function (evt, state) {
                    scope.viewFooter = !state;
                });
            }
        }
    }
})();

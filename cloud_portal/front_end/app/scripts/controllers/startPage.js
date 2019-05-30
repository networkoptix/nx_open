(function () {

    "use strict";

    angular
        .module('cloudApp')
        .controller('StartPageCtrl', StartPage);

    StartPage.$inject = [ '$scope', 'cloudApi', '$location', '$routeParams',
        'dialogs', 'account', 'authorizationCheckService',
        'languageService', 'nxPageService' ];

    function StartPage($scope, cloudApi, $location, $routeParams,
                       dialogs, account, authorizationCheckService,
                       languageService, nxPageService) {

        if ($routeParams.callLogin) {
            nxPageService.setPageTitle(languageService.lang.pageTitles.login);
            dialogs.login(false);
        } else {
            authorizationCheckService.redirectAuthorised();
            $scope.userEmail = account.getEmail();
        }
    }
})();

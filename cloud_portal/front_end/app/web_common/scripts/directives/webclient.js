/**
 * Created by evgeniybalashov on 20/05/17.
 */
angular.module('nxCommon')
    .directive('webclient', function () {
        return {
            restrict: 'A',
            scope:{
                'system': '='
            },
            controller:'ViewCtrl',
            templateUrl: Config.viewsDirCommon + 'components/view.html'
        }
    });
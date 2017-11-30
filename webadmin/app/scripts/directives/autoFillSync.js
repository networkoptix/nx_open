// http://stackoverflow.com/questions/14965968/angularjs-browser-autofill-workaround-by-using-a-directive
angular.module('webadminApp').directive('autoFillSync',['$timeout', function($timeout) {
    return {
        require: 'ngModel',
        link: function(scope, elem, attrs, ngModel) {
            var origVal = elem.val();
            $timeout(function () {
                var newVal = elem.val();
                if(ngModel.$pristine && origVal !== newVal) {
                    ngModel.$setViewValue(newVal);
                }
            }, 500);
        }
    }
}]);
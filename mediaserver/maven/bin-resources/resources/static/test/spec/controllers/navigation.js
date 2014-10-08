'use strict';

describe('Controller: NavigationCtrl', function () {

    // load the controller's module
    beforeEach(module('webadminApp'));

    var NavigationCtrl,
        scope,
        location;

    // Initialize the controller and a mock scope
    beforeEach(inject(function ($controller, $rootScope, $location) {
        location = $location;
        scope = $rootScope.$new();
        NavigationCtrl = $controller('NavigationCtrl', {
            $scope: scope
        });
    }));

    describe('isActive', function () {
        it('returns true when paths are the same', function () {
            location.path('/test');
            expect(scope.isActive('/test')).toBeTruthy();
        });

        it('returns false when paths are different', function () {
            location.path('/different');
            expect(scope.isActive('/test')).toBeFalsy();
        });

        it('returns true if it starts with the same word', function () {
            location.path('/test/1/show');
            expect(scope.isActive('/test')).toBeTruthy();
        });

        it('returns true if it starts with the same word followed by query string', function () {
            location.path('/test?id=1');
            expect(scope.isActive('/test')).toBeTruthy();
        });
    });
});

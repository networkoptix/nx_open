'use strict';

describe('Directive: pwcheck', function () {

    // load the directive's module
    beforeEach(module('webadminApp'));

    var element,
        scope;

    beforeEach(inject(function ($rootScope) {
        scope = $rootScope.$new();
    }));

    it('should make hidden element visible', inject(function ($compile) {
        element = angular.element('<input pw-check="">');
        element = $compile(element)(scope);
        expect(element.text()).toBe('');
    }));
});

'use strict';
var Page = require('./po.js');
describe('Help Page', function () {

    var p = new Page();

    it("should show some links on the left",function(){
        p.get();
        expect (p.links.count()).toBeGreaterThan(0);
        var firstLink = p.links.first().element(by.tagName("a"));
        expect(firstLink.isPresent()).toBe(true);
    });
});
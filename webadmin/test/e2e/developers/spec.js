'use strict';
var Page = require('./po.js');
describe('Developers PAge', function () {

    var p = new Page();
    it("Module",function(){
        expect("all tests").toBe("written");
    });

});
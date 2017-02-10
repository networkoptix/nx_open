'use strict';
var Customization = require('./po.js');
describe('Customized confirmation email', function () {
    var p = new Customization();

    it("should have header with text that matches specific portal", function () {
        var userEmail = p.helper.getRandomEmail();
        p.helper.createUserCustom(p.brandText, p.brandText+'Name', p.brandText+'LastName', userEmail);
    });

    it("should have header with text that matches specific portal if language is changed", function () {
        var userEmail = p.helper.getRandomEmail();
        p.helper.get();
        p.languageDropdown.click();
        p.russianOption.click();
        browser.sleep(2000); // browser.wait and other fancy promises don't help http://docsplendid.com/tags/wait-for-element
        expect(p.languageDropdown.getText()).toContain("Russia");

        p.helper.createUserCustom(p.brandText, p.brandText+'Name', p.brandText+'LastName', userEmail);
    });
});


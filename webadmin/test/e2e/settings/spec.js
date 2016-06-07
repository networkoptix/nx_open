'use strict';

var SettingsPage = require('./po.js');
describe('Settings Page', function () {

    var p = new SettingsPage();

    beforeEach(function() {
        p.get();
    });

    it("Change port: should not allow to set empty or bad port",function(){
        p.setPort(100000);
        expect(p.portInput.getAttribute('class')).toMatch('ng-invalid');
        expect(p.saveButton.isEnabled()).toBe(false);

        p.setPort('');
        expect(p.portInput.getAttribute('class')).toMatch('ng-invalid');
        expect(p.saveButton.isEnabled()).toBe(false);

        p.setPort('port');
        expect(p.portInput.getAttribute('class')).toMatch('ng-invalid');
        expect(p.saveButton.isEnabled()).toBe(false);

        p.setPort(7000);
        expect(p.portInput.getAttribute('class')).not.toMatch('ng-invalid');
        expect(p.saveButton.isEnabled()).toBe(true);
    });

    xit("Change port: should not save allocated port",function() {
        p.setPort(80);

        p.saveButton.click().then(function() {

            expect(p.dialog.getText()).toContain("Error");
            p.dialogButtonClose.click();
        });
    });

    xit("Change port: should save port and show message about restart",function(){
        p.setPort(7000);
        p.saveButton.click().then(function() {
            browser.get('http://10.0.3.241:7000/#/settings');
            // Log in again
            element(by.model('user.username')).sendKeys('admin');
            element(by.model('user.password')).sendKeys('admin');
            element(by.buttonText('Log in')).click();
            browser.sleep(1000);
            expect(element(by.cssContainingText('h2','Server Settings')).isPresent()).toBe(true);
            p.setPort(7001);
            browser.sleep(1000);

            p.saveButton.click().then(function() {
                expect(browser.getCurrentUrl()).toContain('7001');
            });
        });
    });

    it("section All servers in system shows list of servers, including current, with the links", function () {
        // There are some links, related to servers array
        expect(p.mediaServersLinks.count()).toBeGreaterThan(0);

        // Check that there is server, marked as current
        expect(p.currentMediaServer.isDisplayed()).toBe(true);

        // Check that current server url opens the same setting page
        p.currentMediaServerLink.click();
        p.helper.performAtSecondTab( function() {
            expect(browser.getCurrentUrl()).toContain('view'); // Check that url is correct
            browser.close();
        });
    });
});
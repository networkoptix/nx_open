'use strict';
var Page = require('./po.js');
describe('Restart Process', function () {
    var p = new Page();

    it("should confirm restart. on dismiss - no restarting",function(){
        p.get();

        p.restartButton.click();
        expect(p.dialog.getText()).toContain("Do you want to restart server now?");
        p.dialogButtonCancel.click();

        expect(p.restartDialog.isPresent()).toBe(false);
    });

    it("should confirm restart. on accept - show restarting dialog",function(){
        p.get();

        p.restartButton.click();
        expect(p.dialog.getText()).toContain("Do you want to restart server now?");
        p.dialogButtonOk.click();
        expect(p.restartDialog.isDisplayed()).toBe(true);
        expect(p.progressBar.isDisplayed()).toBe(true);
        expect(p.progressBar.getText()).toContain("server is restarting");
        browser.sleep(20000); // wait for restart to finish
    });
});
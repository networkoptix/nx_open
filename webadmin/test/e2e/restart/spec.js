'use strict';
var Page = require('./po.js');
describe('Restart Process', function () {
    //it("should stop test",function(){expect("this test").toBe("runned separately");});return;

    var p = new Page();

    var ptor = protractor.getInstance();

    it("should confirm restart. on dismiss - no restarting",function(){
        p.get();
        p.restartButton.click().then(function(){
            var alertDialog = ptor.switchTo().alert(); // Expect alert.then( // <- this fixes the problem
            expect(alertDialog.getText()).toContain("Do you want to restart server now?");
            alertDialog.dismiss();
            expect(p.restartDialog.isPresent()).toBe(false);
        });
    });

    it("should confirm restart. on accept - show restarting dialog",function(){
        p.restartButton.click().then(function(){
            var alertDialog = ptor.switchTo().alert(); // Expect alert.then( // <- this fixes the problem
            expect(alertDialog.getText()).toContain("Do you want to restart server now?");
            alertDialog.accept().then(function(){
                expect(p.restartDialog.isDisplayed()).toBe(true);
            });
        });
    });

    it("should show progress bar",function(){
        expect(p.progressBar.isDisplayed()).toBe(true);
    });


    it("should show text 'server is offline'",function(){
        browser.wait(function() {
            var d = protractor.promise.defer();

            var callfunc = function () {
                p.progressBar.getText().then(function (text) {
                    if (text.contains("offline")) {
                        d.fulfill(true);
                        done();
                    } else {
                        callfunc();
                    }
                });
            };
            callfunc();
            return d.promise;
        },8000);
        expect(p.progressBar.getText()).toContain("offline");
    });

    it("should refresh automatically after some waiting",function(){
        browser.wait(function() {
            var d = protractor.promise.defer();
            var callfunc = function () {
                p.restartDialog.isDisplayed().then(function (val) {
                    if (val == true) {
                        d.fulfill(true);
                        done();
                    } else {
                        callfunc();
                    }
                });
            };
            callfunc();
            return d.promise;
        },30000);//30 seconds to restart

        expect(p.restartDialog.isDisplayed()).toBe(false);
    });

    it("refresh button should refresh page",function(){
        expect("this").toBe("tested mnually");
    });
});
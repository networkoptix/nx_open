'use strict';

describe('Navigation Menu', function() {
    var navbarElems = element(by.css('.navbar-right')).all(by.css('a'));

    it('changes active link depending on route', function() {
        var linkTexts = ['Web client','Settings', 'Information', 'For Developers', 'Help'];
        browser.get('/#/settings');
        browser.waitForAngular();

        navbarElems.each (function(element, index){
            element.getText().then( function(text) {
                if (text.length > 0) {
                    expect(text).toContain(linkTexts[index]);
                    element.click();
                    expect(element.element(by.xpath('..')).getAttribute('class')).toContain('active');
                }
            });
        });
    });

    it('shows essential server info in title', function() {
        var serverTitle = element(by.css(".navbar-brand"));
        expect(serverTitle.getAttribute("title")).toMatch(/Server\sname\:\s[\d\w\s*].*\nVersion\:\s\d+\.\d+\.\d+\.\d+\s*[\d\w\s*].*\nIP\:\s(\d+\.\d+\.\d+\.\d+\s*)/); // add Version regex
    });
});

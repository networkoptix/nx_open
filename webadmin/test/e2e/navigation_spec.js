'use strict';

describe('Navigation Menu', function() {


    //it("should stop test",function(){expect("other test").toBe("uncommented");});return;


    it('changes active link depending on route', function() {
    browser.get('/');

    expect(element.all(by.css('.active')).first().getText()).toEqual('Web client');

    var linkTexts = ['Web client','Settings', 'Information', 'For Developers', 'Help'];
    for (var i = 0; i < linkTexts.length; i++) {
      var linkText = linkTexts[i];

      var link = element(by.linkText(linkText));
      link.click();

      expect(element(by.css('nav[role=navigation]')).element(by.css('.active')).getText()).toEqual(linkText);
    }
  });

    it('shows essential server info in title', function() {
        var servertitle = element(by.css(".navbar-brand"));
        expect(servertitle.getAttribute("title")).toMatch(/Server\sname\:\s[\d\w\s*].*\nVersion\:\s\d+\.\d+\.\d+\.\d+\s*[\d\w\s*].*\nIP\:\s(\d+\.\d+\.\d+\.\d+\s*)/); // add Version regex
    });
});

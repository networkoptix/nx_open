'use strict';

describe('Navigation Menu', function() {


    it("Merge: should show clickable button",function(){
        expect("other test").toBe("uncommented");
    });
    return;

  it('changes active link depending on route', function() {
    browser.get('/');

    expect(element(by.css('.active')).getText()).toEqual('Settings');

    var linkTexts = ['Information', 'Web client', 'For Developers', 'Help'];
    for (var i = 0; i < linkTexts.length; i++) {
      var linkText = linkTexts[i];

      var link = element(by.linkText(linkText));
      link.click();

      expect(element(by.css('.active')).getText()).toEqual(linkText);
    }
  });


    it('shows essential server info in title', function() {
        expect("test").toBe("written");
    });
});

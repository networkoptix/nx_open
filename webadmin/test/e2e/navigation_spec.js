'use strict';

describe('Navigation Menu', function() {
  it('changes active link depending on route', function() {
    browser.get('/');

    // We expect that initially Settings link is active
    var activeListItem = element(by.css('.active'));
    var text = activeListItem.findElement(by.tagName('a')).getText();

    expect(text).toEqual('Settings');

    var linkTexts = ['Information', 'For Developers', 'Support', 'Help'];
    for (var i = 0; i < linkTexts.length; i++) {
      var linkText = linkTexts[i];

      var link = element(by.linkText(linkText));
      link.click();
      activeListItem = element(by.css('.active'));
      text = activeListItem.findElement(by.tagName('a')).getText();

      expect(text).toEqual(linkText);
    }
  });
});

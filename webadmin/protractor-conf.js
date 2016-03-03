exports.config = {
  // Do not start a Selenium Standalone sever - only run this using chrome.

  //chromeOnly: true,
  //chromeDriver: 'node_modules/protractor/selenium/chromedriver',

  baseUrl: 'http://127.0.0.1:10000',

  // Capabilities to be passed to the webdriver instance.
  capabilities: {
      'browserName': 'chrome',
      'chromeOptions': {
          'args': ['--test-type']
      }
  },
  seleniumAddress: 'http://localhost:4444/wd/hub',

  // Spec patterns are relative to the current working directly when
  // protractor is called.
  specs: ['test/e2e/**/*spec.js'],
  // specs: ['test/e2e/settings/*spec.js'],

  // Options to be passed to Jasmine-node.
  jasmineNodeOpts: {
    showColors: true,
    defaultTimeoutInterval: 30000
  },

  // Authentication before running tests
  onPrepare: function() {
    browser.get('/');
    browser.waitForAngular();

    element(by.model('user.username')).sendKeys('admin');
    element(by.model('user.password')).sendKeys('admin');
    element(by.buttonText('Log in')).click();

    // Login takes some time, so wait until it's done. Todo: think about better wait command
    return browser.driver.wait(function() {
      return browser.driver.getCurrentUrl();
    }, 10000);
  }
};

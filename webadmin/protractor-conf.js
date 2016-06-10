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
    // specs: ['test/e2e/info/*spec.js'],

    // Options to be passed to Jasmine-node.
    jasmineNodeOpts: {
        showColors: true,
        defaultTimeoutInterval: 30000
    },

    // Authentication before running tests
    onPrepare: function() {
        var CustomReporter = require('./custom_reporter.js');
        jasmine.getEnv().addReporter(CustomReporter);

        browser.get('/');
        browser.waitForAngular();

        element(by.model('user.username')).sendKeys('admin');
        element(by.model('user.password')).sendKeys('admin');
        element(by.buttonText('Log in')).click();

        browser.sleep(500);
    }
};

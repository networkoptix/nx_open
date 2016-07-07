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
    //specs: ['test/e2e/**/*spec.js'],
    specs: ['test/e2e/setup/*spec.js'],

    // Options to be passed to Jasmine-node.
    jasmineNodeOpts: {
        showColors: true,
        defaultTimeoutInterval: 30000
    },

    // Authentication before running tests
    onPrepare: function() {
        var CustomReporter = require('./custom_reporter.js');
        jasmine.getEnv().addReporter(CustomReporter);
        var Helper = require('./test/e2e/helper.js');
        this.helper = new Helper();

        var self = this;

        browser.get('/');
        browser.waitForAngular();

        this.helper.checkPresent(this.helper.setupWizardDialog).then( self.helper.completeSetup,
            function() { // if there's no setup wizard
                element(by.model('user.username')).sendKeys('admin');
                element(by.model('user.password')).sendKeys('qweasd123');
                element(by.buttonText('Log in')).click();

                browser.sleep(500);
            });
    }
};

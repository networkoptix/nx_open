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
    //specs: ['test/e2e/info/*spec.js'],

    // Options to be passed to Jasmine-node.
    jasmineNodeOpts: {
        showColors: true,
        // adjust id you see message "Timeout - Async callback was not invoked within timeout specified by jasmine.DEFAULT_TIMEOUT_INTERVAL"
        defaultTimeoutInterval: 90000
    },

    // Waiting for Page Synchronization https://github.com/angular/protractor/blob/master/docs/timeouts.md
    // Adjust if you see message "Timed out waiting for Protractor to synchronize with the page after 60 seconds"
    allScriptsTimeout: 60000,

    // Authentication before running tests
    onPrepare: function() {
        var CustomReporter = require('./custom_reporter.js');
        jasmine.getEnv().addReporter(CustomReporter);
        var Helper = require('./test/e2e/helper.js');
        this.helper = new Helper();

        var self = this;

        browser.get('/');
        browser.waitForAngular();

        //  If setup wizard is present, complete setup
        self.helper.setupWizardDialog.isPresent().then( function(isPresent) {
            if(isPresent) {
                self.helper.completeSetup(); }
        });

        var pwd = self.helper.password;
        self.helper.attemptLogin([self.helper.admin, pwd], [self.helper.cloudEmail, pwd]);
    }
};

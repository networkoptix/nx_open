var customization = require('./test-customizations.json');
exports.config = {


    // Do not start a Selenium Standalone sever - only run this using chrome.

    //chromeOnly: true,
    //chromeDriver: 'node_modules/protractor/selenium/chromedriver',

    //baseUrl: 'https://localhost:9000', // Local grunt serve
    baseUrl: 'http://cloud-test.hdw.mx',
    //baseUrl: 'http://cloud-local', // local vagrant
    //baseUrl: customization.default.portal, // get portal url from ./test-customization.json (filled by grunt testallbrands)


    // Capabilities to be passed to the webdriver instance.
    capabilities: {
        'browserName': 'chrome',
        'chromeOptions': {
            'args': ['--test-type','--ignore-certificate-errors']
        }
    },

/*    multiCapabilities: [{
        'browserName': 'chrome',
        'chromeOptions': {
            'args': ['--test-type']
        }
    }, {
        'browserName': 'firefox'
    }],
*/

    seleniumAddress: 'http://localhost:4444/wd/hub',

    // Spec patterns are relative to the current working directly when
    // protractor is called.
    //specs: ['test/e2e/**/*spec.js'],
    //specs: ['test/e2e/login/login*.js','test/e2e/login/*neg*.js','test/e2e/system_page/share*.js'],
    specs: ['test/e2e/login/*neg*.js'],
    //specs: ['test/e2e/system_page/share*.js'],

    // Options to be passed to Jasmine-node.
    jasmineNodeOpts: {
        showColors: true,
        defaultTimeoutInterval: 120000
    },

    allScriptsTimeout: 30000, // by default it is 11s. See Waiting For Page Sync http://www.protractortest.org/#/timeouts

    onPrepare: function () {
        var CustomReporter = require('./custom_reporter.js');
        jasmine.getEnv().addReporter(CustomReporter);

        // Docs at https://www.npmjs.com/package/mail-notifier
        var notifier = require('mail-notifier');

        var imap = {
            user: "noptixqa@gmail.com",
            password: "qweasd!@#",
            host: "imap.gmail.com",
            port: 993,
            tls: true,
            tlsOptions: { rejectUnauthorized: false }
        };

        global.notifier = notifier(imap);
    }
};

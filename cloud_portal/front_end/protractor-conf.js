// An example configuration file.
exports.config = {
    // Do not start a Selenium Standalone sever - only run this using chrome.

    //chromeOnly: true,
    //chromeDriver: 'node_modules/protractor/selenium/chromedriver',

    baseUrl: 'http://127.0.0.1:9000',
    //baseUrl: 'http://cloud-demo.hdw.mx',


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
    //specs: ['test/e2e/system_page/*spec.js'],

    // Options to be passed to Jasmine-node.
    jasmineNodeOpts: {
        showColors: true,
        defaultTimeoutInterval: 60000
    },

    onPrepare: function () {
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

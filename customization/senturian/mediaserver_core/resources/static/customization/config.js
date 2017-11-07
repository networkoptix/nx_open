Config.helpLinks.push({
    url: "info@senturiansolutions.com",
    title: "Support",
    description:"Have a question about specific features of your system?",
    button:"get support",
    target: "new" // new|frame
});

Config.helpLinks.push({
    urls: [
        {
            url: "https://itunes.apple.com/us/app/sentry-matrix/id1209154241?ls=1&mt=8",
            button: "iOS Client",
            class:'appstore'
        },
        {
            url: "https://play.google.com/store/apps/details?id=com.senturiansolutions.sentry",
            class:'googleplay',
            button: "Android Client"
        }
    ],
    title: "Mobile Apps",
    target: "new" // new|frame
});

Config.allowDebugMode = false;
Config.productName = 'Sentry Matrix';
Config.cloud.productName = 'Sentry Cloud';

Config.supportedLanguages = ['en_US', 'zh_CN'];
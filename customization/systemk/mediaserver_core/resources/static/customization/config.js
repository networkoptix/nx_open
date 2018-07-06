Config.helpLinks.push({
    url: "marketing@systemk.co.jp",
    title: "Support",
    description:"Have a question about specific features of your system?",
    button:"get support",
    target: "new" // new|frame
});

Config.helpLinks.push({
    urls: [
        {
            url: "https://itunes.apple.com/eg/app/hd-witness/id1227909757?mt=8",
            button: "iOS Client",
            class:'appstore'
        },
        {
            url: "https://play.google.com/store/apps/details?id=jp.co.systemk.skvms",
            class:'googleplay',
            button: "Android Client"
        }
    ],
    title: "Mobile Apps",
    target: "new" // new|frame
});

Config.allowDebugMode = false;
Config.productName = 'SK_VMS';
Config.cloud.productName = 'SK Cloud';
Config.defaultLanguage = 'ja_JP';
Config.supportedLanguages = ['ja_JP'];
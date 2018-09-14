Config.helpLinks.push({
    url: "support.xstream@xccelo.com",
    title: "Support",
    target: "new" // new|frame
});

Config.helpLinks.push({
    url: "http://networkoptix.com/calculator/#/",
    title: "System calculator",
    target: "new" // new|frame
});

Config.helpLinks.push({
    urls: [
        {
            url: "https://itunes.apple.com/us/app/xstream-mobile/id1398591536?ls=1&mt=8",
            button: "iOS Client",
            class:'appstore'
        },
        {
            url: "https://play.google.com/store/apps/details?id=com.xccelo.xstream",
            class:'googleplay',
            button: "Android Client"
        }
    ],
    title: "Mobile Apps",
    target: "new" // new|frame
});

Config.allowDebugMode = false;
//Config.webclientEnabled = false; // Uncomment this string to disable web client
Config.productName = 'Xstream';
Config.cloud.productName = 'Xstream Cloud';
Config.defaultLanguage = 'de_DE';
Config.supportedLanguages = ['en_US', 'en_GB', 'fr_FR', 'de_DE', 'ru_RU', 'es_ES', 'ja_JP', 'ko_KR', 'tr_TR', 'zh_CN', 'zh_TW', 'hu_HU', 'he_IL', 'nl_NL', 'pl_PL', 'vi_VN', 'th_TH'];
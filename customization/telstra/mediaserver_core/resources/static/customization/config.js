Config.helpLinks.push({
    url: "managedvideosurveillance@team.telstra.com",
    title: "Support",
    description:"Have a question about specific features of your system?",
    button:"get support",
    target: "new" // new|frame
});

Config.helpLinks.push({
    urls: [
        {
            url: "https://itunes.apple.com/app/id1438062729",
            button: "iOS Client",
            class:'appstore'
        },
        {
            url: "https://play.google.com/store/apps/details?id=com.telstra.telstra",
            class:'googleplay',
            button: "Android Client"
        }
    ],
    title: "Mobile Apps",
    target: "new" // new|frame
});

Config.allowDebugMode = false;
Config.productName = 'Telstra Video Surveillance';
Config.cloud.productName = 'Telstra Video Surveillance';
Config.defaultLanguage = 'en_US';
Config.supportedLanguages = ['en_US', 'en_GB', 'fr_FR', 'de_DE', 'ru_RU', 'es_ES', 'ja_JP', 'ko_KR', 'tr_TR', 'zh_CN', 'zh_TW', 'hu_HU', 'he_IL', 'nl_NL', 'pl_PL', 'vi_VN', 'th_TH'];
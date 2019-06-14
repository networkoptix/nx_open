Config.helpLinks.push({
    url: "mailto:support@coresmp.com",
    title: "Support",
    description:"Have a question about specific features of your system?",
    button:"get support",
    target: "new" // new|frame
});

Config.helpLinks.push({
    urls: [
        {
            url: "https://itunes.apple.com/app/id1466148931",
            button: "iOS Client",
            class:'appstore'
        },
        {
            url: "https://play.google.com/store/apps/details?id=com.comm_core.coresmp",
            class:'googleplay',
            button: "Android Client"
        }
    ],
    title: "Mobile Apps",
    target: "new" // new|frame
});

Config.allowDebugMode = false;
Config.productName = 'COREsmp';
Config.cloud.productName = 'COREconnect';
Config.defaultLanguage = 'en_US';
Config.supportedLanguages = ['en_US', 'en_GB', 'fr_FR', 'de_DE', 'ru_RU', 'es_ES', 'ja_JP', 'ko_KR', 'tr_TR', 'zh_CN', 'zh_TW', 'hu_HU', 'he_IL', 'nl_NL', 'pl_PL', 'vi_VN', 'th_TH'];

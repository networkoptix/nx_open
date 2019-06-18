Config.helpLinks.push({
    url: "support@pikovms.com",
    description:"Have a question about specific features of your system?",
    button:"get support",
    target: "new" // new|frame
});

Config.helpLinks.push({
    urls: [
        {
            url: "https://apps.apple.com/us/app/piko-vms/id1458431603",
            button: "iOS Client",
            class:'appstore'
        },
        {
			url: "https://play.google.com/store/apps/details?id=com.pikovms.piko",
			title: "Android Client",
			target: "new" // new|frame
        }
    ],
    title: "Mobile Apps",
    target: "new" // new|frame
});

Config.allowDebugMode = false;
//Config.webclientEnabled = false; // Uncomment this string to disable web client
Config.productName = 'Piko';
Config.cloud.productName = 'Piko Cloud';
Config.defaultLanguage = 'en_US';
Config.supportedLanguages = ['en_US', 'en_GB', 'fr_FR', 'de_DE', 'ru_RU', 'es_ES', 'ja_JP', 'ko_KR', 'tr_TR', 'zh_CN', 'zh_TW', 'hu_HU', 'he_IL', 'nl_NL', 'pl_PL', 'vi_VN', 'th_TH'];
Config.helpLinks.push({
    url: "mailto:support@ioezio.com",
    title: "EZ Pro Support",
    target: "new",
    button: "support@ioezio.com
});

Config.helpLinks.push({
    urls: [
        {
            url: "https://apps.apple.com/us/app/ez-pro-mobile/id1167886315?ls=1",
            button: "iOS Client",
            class:'appstore'
        },
        {
			url: "https://play.google.com/store/apps/details?id=com.ioezio.ezpromobile",
			title: "Android Client",
			target: "new" // new|frame
        }
    ],
    title: "Mobile Apps",
    target: "new" // new|frame
});

Config.allowDebugMode = false;
//Config.webclientEnabled = false; // Uncomment this string to disable web client
Config.productName = 'EZ Pro';
Config.cloud.productName = 'EZ Pro Cloud';
Config.defaultLanguage = 'en_US';
Config.supportedLanguages = ['en_US', 'en_GB', 'fr_FR', 'de_DE', 'ru_RU', 'es_ES', 'ja_JP', 'ko_KR', 'tr_TR', 'zh_CN', 'zh_TW', 'hu_HU', 'he_IL', 'nl_NL', 'pl_PL', 'vi_VN', 'th_TH'];
Config.developersFeedbackForm = false;
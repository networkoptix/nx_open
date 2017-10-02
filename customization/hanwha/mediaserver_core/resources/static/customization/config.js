Config.helpLinks.push({
    url: "mailto:support@hanwhasecurity.com",
    title: "Support",
    target: "new" // new|frame
});

Config.helpLinks.push({
    url: "https://www.hanwhasecurity.com/wavecalculator/",
    title: "System calculator",
    target: "new" // new|frame
});

Config.helpLinks.push({
    urls: [
        {
            url: "https://itunes.apple.com/us/app/wave-mobile/id1284037424?ls=1&mt=8",
            button: "iOS Client",
            class:'appstore'
        },
        {
            url: "https://play.google.com/store/apps/details?id=com.hanwhasecurity.wavemobile",
            class:'googleplay',
            button: "Android Client"
        }
    ],
    title: "Mobile Apps",
    target: "new" // new|frame
});

Config.allowDebugMode = false;
//Config.webclientEnabled = false; // Uncomment this string to disable web client
Config.productName = 'WAVE';
Config.cloud.productName = 'WAVE Sync';

Config.supportedLanguages = ['en_US', 'en_GB', 'fr_FR', 'de_DE', 'ru_RU', 'es_ES', 'ja_JP', 'ko_KR', 'tr_TR', 'zh_CN', 'zh_TW', 'hu_HU', 'he_IL', 'nl_NL', 'pl_PL', 'vi_VN'];

Config.developersFeedbackForm = 'https://docs.google.com/forms/d/e/1FAIpQLSfTomScw5Me1oQERFPs4sNnLXGboCkFvHzF1ZgLkPlAIjOsXg/viewform?usp=pp_url&entry.1099959647={{PRODUCT}}';

Config.helpLinks.push({
    url: "http://www.ubiquitysmart.com/",
    title: "Support",
    description:"Have a question about specific features of your system?",
    button:"get support",
    target: "new" // new|frame
});

Config.helpLinks.push({
    urls: [
        {
            url: "https://itunes.apple.com/us/app/ust-vms-viewer/id1120424275?ls=1&mt=8",
            button: "iOS Client",
            class:'appstore'
        },
        {
            url: "https://play.google.com/store/apps/details?id=com.networkoptix.nxwitness",
            class:'googleplay',
            button: "Android Client"
        }
    ],
    title: "Mobile Apps",
    target: "new" // new|frame
});

Config.allowDebugMode = false;
//Config.webclientEnabled = false; // Uncomment this string to disable web client
Config.productName = 'VMS DTV';
Config.cloud.productName = 'DTV Cloud';

Config.supportedLanguages = ['en_US', 'en_GB', 'fr_FR', 'de_DE', 'ru_RU', 'es_ES', 'ja_JP', 'ko_KR', 'tr_TR', 'zh_CN', 'zh_TW', 'hu_HU', 'he_IL', 'nl_NL', 'pl_PL', 'vi_VN', 'th_TH'];
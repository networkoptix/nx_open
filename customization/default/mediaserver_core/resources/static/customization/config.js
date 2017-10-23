Config.helpLinks.push({
    url: "http://support.networkoptix.com",
    title: "Support",
    description:"Have a question about specific features of your Nx Witness system?",
    button:"get support",
    target: "new" // new|frame
});

Config.helpLinks.push({
    url: "http://networkoptix.com/calculator/#/",
    title: "System calculator",
    description:"Building a new Nx Witness system or expanding your current system? Use the Nx System Calculator to calculate suggested storage and network requirements.",
    button: "Go to the calculator",
    target: "new" // new|frame
});


Config.helpLinks.push({
    urls: [
        {
            url: "https://itunes.apple.com/eg/app/hd-witness/id1050899754?mt=8",
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
Config.productName = 'Nx Witness';
Config.cloud.productName = 'Nx Cloud';

Config.supportedLanguages = ['en_US', 'en_GB', 'fr_FR', 'de_DE', 'ru_RU', 'es_ES', 'ja_JP', 'ko_KR', 'tr_TR', 'zh_CN', 'zh_TW', 'hu_HU', 'he_IL', 'nl_NL', 'pl_PL', 'vi_VN', 'th_TH'];
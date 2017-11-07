Config.helpLinks.push({
    url: "http://digital-watchdog.com/support/contact-tech-support/",
    title: "Support",
    description:"Have a question about specific features of your VMS system?",
    button:"get support",
    target: "new" // new|frame
});

Config.helpLinks.push({
    url: "http://digital-watchdog.com/support/tools/ip-configurator/",
    title: "System calculator",
    description:"Building a new VMS system or expanding your current system? Use the System Calculator to calculate suggested storage and network requirements.",
    button: "Go to the calculator",
    target: "new" // new|frame
});

Config.helpLinks.push({
    urls: [
        {
            url: "https://itunes.apple.com/us/app/dw-spectrum/id1090087818?mt=8",
            button: "iOS Client",
            class:'appstore'
        },
        {
            url: "https://play.google.com/store/apps/details?id=com.digitalwatchdog.dwmobile",
            class:'googleplay',
            button: "Android Client"
        }
    ],
    title: "Mobile Apps",
    target: "new" // new|frame
});

Config.allowDebugMode = false;
//Config.webclientEnabled = false; // Uncomment this string to disable web client
Config.productName = 'DW Spectrum';
Config.cloud.productName = 'DW Cloud';

Config.supportedLanguages = ['en_US', 'en_GB', 'fr_FR', 'de_DE', 'ru_RU', 'es_ES', 'ja_JP', 'ko_KR', 'tr_TR', 'zh_CN', 'zh_TW', 'hu_HU', 'he_IL', 'nl_NL', 'pl_PL', 'vi_VN', 'th_TH'];
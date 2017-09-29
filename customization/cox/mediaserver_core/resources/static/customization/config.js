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
            url: "https://itunes.apple.com/us/app/cox-business-security/id904940952?mt=8",
            button: "iOS Client",
            class:'appstore'
        },
        {
            url: "https://play.google.com/store/apps/details?id=com.cox.hdsec",
            class:'googleplay',
            button: "Android Client"
        }
    ],
    title: "Mobile Apps",
    target: "new" // new|frame
});

Config.allowDebugMode = false;
//Config.webclientEnabled = false; // Uncomment this string to disable web client
Config.productName = 'Cox Business Security Solutions';
Config.cloud.productName = 'Cox Cloud';

Config.supportedLanguages = ['en_US', 'en_GB', 'fr_FR', 'de_DE', 'ru_RU', 'es_ES', 'ja_JP', 'ko_KR', 'tr_TR', 'zh_CN', 'zh_TW', 'hu_HU', 'he_IL', 'nl_NL', 'pl_PL', 'vi_VN'];
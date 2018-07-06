Config.helpLinks.push({
    url: "http://vista-cctv.com/technical-support/contact/",
    title: "Support",
    description:"Have a question about specific features of your VMS system?",
    button:"Get support",
    target: "new" // new|frame
});

Config.helpLinks.push({
    url: "http://vista-cctv.com/qulu_calculator/#/",
    title: "System calculator",
    description:"Building a new VMS system or expanding your current system? Use the System Calculator to calculate suggested storage and network requirements.",
    button:"Go to the calculator",
    target: "new" // new|frame
});



Config.helpLinks.push({
    urls: [
        {
            url: "https://itunes.apple.com/us/app/qulu/id1090487857?mt=8",
            button: "iOS Client",
            class:'appstore'
        },
        {
            url: "https://play.google.com/store/apps/details?id=com.vista.qulumobile&hl=en",
            class:'googleplay',
            button: "Android Client"
        }
    ],
    title: "Mobile Apps",
    target: "new" // new|frame
});

Config.allowDebugMode = false;
//Config.webclientEnabled = false; // Uncomment this string to disable web client
Config.productName = 'Qulu';
Config.cloud.productName = 'Qloud';
Config.defaultLanguage = 'en_GB';
Config.supportedLanguages = ['en_GB', 'fr_FR', 'de_DE', 'ru_RU', 'es_ES', 'hu_HU', 'nl_NL'];
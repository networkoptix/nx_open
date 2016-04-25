Config.helpLinks.push({
    url: "http://vista-cctv.com/technical-support/contact/",
    title: "Support",
    description:"Have a question about specific features of your VMS system?",
    button:"get support",
    target: "new" // new|frame
});

Config.helpLinks.push({
    url: "http://vista-cctv.com/qulu_calculator/#/",
    title: "Hardware calculator",
    description:"Building a new VMS system or expanding your current system? Use the Hardware Calculator to calculate suggested storage and network requirements.",
    button:"calculate",
    target: "new" // new|frame
});



Config.helpLinks.push({
    urls: [
        {
            url: "https://itunes.apple.com/us/app/qulu/id888320566?mt=8",
            button: "iOS Client",
            class:'appstore'
        },
        {
            url: "https://play.google.com/store/apps/details?id=com.vista.qulu&hl=en",
            class:'googleplay',
            button: "Android Client"
        }
    ],
    title: "Mobile Apps",
    description:"View live and recorded video from your VMS system on your iOs or Android mobile device",
    target: "new" // new|frame
});

Config.allowDebugMode = false;
//Config.webclientEnabled = false; // Uncomment this string to disable web client

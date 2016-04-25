Config.helpLinks.push({
    url: "http://digital-watchdog.com/support/contact-tech-support/",
    title: "Support",
    description:"Have a question about specific features of your VMS system?",
    button:"get support",
    target: "new" // new|frame
});

Config.helpLinks.push({
    url: "http://digital-watchdog.com/support/tools/ip-configurator/",
    title: "Hardware calculator",
    description:"Building a new VMS system or expanding your current system? Use the Hardware Calculator to calculate suggested storage and network requirements.",
    button:"calculate",
    target: "new" // new|frame
});

Config.helpLinks.push({
    urls: [
        {
            url: "https://itunes.apple.com/us/app/dw-spectrum/id648577856?mt=8",
            button: "iOS Client",
            class:'appstore'
        },
        {
            url: "https://play.google.com/store/apps/details?id=com.digitalwatchdog.dwspectrum",
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

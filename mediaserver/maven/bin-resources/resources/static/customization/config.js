Config.helpLinks.push({
    url: "http://support.networkoptix.com",
    title: "Support",
    description:"Have a question about specific features of your Nx Witness system?",
    button:"get support",
    target: "new" // new|frame
});

Config.helpLinks.push({
    url: "http://networkoptix.com/calculator/#/",
    title: "Hardware calculator",
    description:"Building a new Nx Witness system or expanding your current system? Use the Nx System Calculator to calculate suggested storage and network requirements.",
    button:"calculate",
    target: "new" // new|frame
});


Config.helpLinks.push({
    urls: [
        {
            url: "https://itunes.apple.com/eg/app/hd-witness/id648369716?mt=8",
            button: "iOS Client",
            class:'appstore'
        },
        {
            url: "https://play.google.com/store/apps/details?id=com.networkoptix.hdwitness",
            class:'googleplay',
            button: "Android Client"
        }
    ],
    title: "Mobile Apps",
    description:"View live and recorded video from your VMS system on your iOs or Android mobile device",
    target: "new" // new|frame
});
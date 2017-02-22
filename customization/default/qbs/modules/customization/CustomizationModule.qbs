import qbs

Module
{
    property string customizationName: "default"
    property string companyName: "Network Optix"
    property string companyNameShort: "Nx"
    property string debCompanyName: "networkoptix"
    property string productName: "HD Witness"
    property string productDisplayName: "Nx Witness"
    property string productNameShort: "hdwitness"
    property string installerName: "nxwitness"

    property bool freeLicenseIsTrial: true
    property int freeLicenseCount: 4
    property string freeLicenseKey: "0000-0000-0000-0005"

    property string cloudHost: "cloud-test.hdw.mx"
    property string cloudName: "Nx Cloud"

    property string liteDeviceName: "Nx1"

    property string helpUrl: "http://networkoptix.com/files/help"
    property string settingsUrl: "http://networkoptix.com/files/settings"
    property string showcaseUrl: "http://networkoptix.com/files/showcase"
    property string testUpdateFeedUrl: "http://updates.hdwitness.com/updates.json"
    property string prodUpdateFeedUrl: "http://updates.hdwitness.com/updates.json"
    property string updateGeneratorUrl: "http://updates.hdw.mx/upcombiner/upcombine"
    property string supportEmail: "support@networkoptix.com"
    property string supportUrl: "http://support.networkoptix.com"
    property string companyUrl: "http://www.networkoptix.com"
    property string licensingEmail: "http://support.networkoptix.com"
    property string uriProtocol: "nx-vms"
    property string mirrorListUrl: "http://downloads.hdwitness.com/clients/mirror.xml"

    property string androidPackageName: "com.networkoptix.nxwitness"
    property string androidOldPackageName: "com.networkoptix.hdwitness"
    property string androidAlias: "hdwitness"
    property string androidStorepass: "hYCmvTDu"
    property string androidKeypass: "31O2zNNy"

    property string iosOldBundleIdentifier: "com.networkoptix.HDWitness"
    property string iosBundleIdentifier: "com.networkoptix.NxMobile"
    property string iosGroupIdentifier: "group.com.networkoptix.NxMobile"
    property string iosSignIdentity: "iPhone Distribution: Network Optix, Inc. (L6FE34GJWM)"
    property string iosOldAppstoreId: "id648369716"
    property string iosAppstoreId: "id1050899754"

    property string backgroundImage:
        '{"enabled": true, "name": ":/skin/background.png", "mode": "Crop", "opacity": "0.03"}'

    property string defaultWebPages:
        '{"Home Page": "http://networkoptix.com", "Support": "http://support.networkoptix.com"}'

    property string macBundleIdentifier: "com.networkoptix.HDWitness2"

    property stringList translations: [
        "en_US", "en_GB",
        "fr",
        "de",
        "ru",
        "es",
        "ja",
        "ko",
        "th",
        "zh_CN", "zh_TW",
        "vi_VN",
        "hu",
        "pl",
        "he",
        "fil",
        "ar",
        "id",
        "tr",
        "km",
        "pt_BR",
        "nl_BE"
    ]
    property string defaultTranslation: translations[0]
}

import qbs

Module
{
    Depends { name: "cpp" }

    property bool enableAllVendors: true
    property bool enableSsl: true
    property bool enableDesktopCamera: true
    property bool enableThirdParty: true
    property bool enableArchive: true
    property bool enableDataProviders: true
    property bool enableSoftwareMotionDetection: true
    property bool enableSendMail: true
    property bool enableMdns: true

    readonly property string apiExportMacro:
        qbs.targetOS.contains("windows") ? "__declspec(dllexport)" : ""
    readonly property string apiImportMacro:
        qbs.targetOS.contains("windows") ? "__declspec(dllimport)" : ""

    // YOU MUST FIX THE LINES BELOW to make sure it will generate merge conflict
    // if somebody also updates the protocol version
    // Changed localSystem
    property int ec2ProtoVersion: 3022

    Properties
    {
        condition: qbs.targetOS.contains("ios")
        enableAllVendors: false
        enableSsl: false
        enableThirdParty: false
        enableMdns: false
        enableArchive: false
        enableDataProviders: false
        enableSoftwareMotionDetection: false
        enableSendMail: false
    }
    Properties
    {
        condition: qbs.targetOS.contains("android")
        enableAllVendors: false
        enableSsl: false
        enableThirdParty: false
        enableMdns: false
        enableArchive: false
        enableDataProviders: false
        enableSoftwareMotionDetection: false
        enableSendMail: false
    }
    Properties
    {
        condition: project.target == "isd" || project.target == "isd_s2"
        enableAllVendors: false
        enableSoftwareMotionDetection: false
        enableDesktopCamera: false
    }

    cpp.defines:
    {
        var defines = [
            "USE_NX_HTTP",
            "__STDC_CONSTANT_MACROS"
        ]

        if (enableSsl)
            defines.push("ENABLE_SSL")
        if (enableSendMail)
            defines.push("ENABLE_SENDMAIL")
        if (enableDataProviders)
            defines.push("ENABLE_DATA_PROVIDERS")
        if (enableDesktopCamera)
            defines.push("ENABLE_DESKTOP_CAMERA")
        if (enableSoftwareMotionDetection)
            defines.push("ENABLE_SOFTWARE_MOTION_DETECTION")
        if (enableThirdParty)
            defines.push("ENABLE_THIRD_PARTY")
        if (enableArchive)
            defines.push("ENABLE_ARCHIVE")
        if (enableMdns)
            defines.push("ENABLE_MDNS")

        if (enableAllVendors)
        {
            defines.push(
                "ENABLE_ONVIF",
                "ENABLE_AXIS",
                "ENABLE_ACTI",
                "ENABLE_ARECONT",
                "ENABLE_DLINK",
                "ENABLE_DROID",
                "ENABLE_TEST_CAMERA",
                "ENABLE_STARDOT",
                "ENABLE_IQE",
                "ENABLE_ISD",
                "ENABLE_PULSE_CAMERA"
            )
        }

        if (qbs.targetOS.contains("windows"))
            defines.push("ENABLE_VMAX")

        if (qbs.targetOS.contains("unix"))
            defines.push("QN_EXPORT=")

        if (project.target == "isd" || project.target == "isd_s2")
            defines.push("EDGE_SERVER")

        return defines
    }

    cpp.cxxFlags:
    {
        var flags = []

        if (qbs.targetOS.contains("unix"))
        {
            flags.push(
                "-W",
                "-Werror=enum-compare",
                "-Werror=reorder",
                "-Werror=delete-non-virtual-dtor",
                "-Werror=return-type",
                "-Werror=conversion-null",
                "-Wuninitialized"
            )
        }

        if (qbs.targetOS.contains("linux"))
        {
            flags.push(
                "-msse2",
                "-Wno-unused-local-typedefs",
                "-Wno-unknown-pragmas",
                "-Wno-ignored-qualifiers"
            )
        }

        return flags
    }
}

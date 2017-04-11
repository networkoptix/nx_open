import qbs
import qbs.FileInfo

GenericProduct
{
    name: "common"

    Depends { name: "mercurialInfo" }

    Depends
    {
        name: "Qt"
        submodules: [
            "core-private",
            "network",
            "xml",
            "xmlpatterns",
            "sql",
            "concurrent",
            "multimedia"
        ]
    }

    Depends { name: "openssl" }
    Depends { name: "ffmpeg" }
    Depends
    {
        name: "quazip"
        condition: project.withDesktopClient || project.withMediaServer
    }
    Depends { name: "boost" }

    Depends { name: "nx_fusion" }

    cpp.includePaths: [
        "src",
        buildDirectory,
        project.sourceDirectory + "/common_libs/nx_network/src",
        project.sourceDirectory + "/common_libs/nx_streaming/src"
    ]

    cpp.allowUnresolvedSymbols: true
    cpp.defines: ["NX_NETWORK_API=" + vms_cpp.apiImportMacro]

    Properties
    {
        condition: qbs.targetOS.contains("macos")
        cpp.frameworks: ["Foundation", "AppKit"]
    }

    Group
    {
        condition: qbs.targetOS.contains("macos")
        files: ["src/utils/mac_utils.mm"]
    }

    Group
    {
        files: "maven/filter-resources/app_info_impl.cpp"
        fileTags: "configure.input"
        configure.outputTags: "cpp"
        configure.replacements: ({
            "nxec.ec2ProtoVersion": vms_cpp.ec2ProtoVersion,
            "company.name": customization.companyName,
            "deb.customization.company.name": customization.debCompanyName,
            "parsedVersion.majorVersion": project.versionMajor,
            "parsedVersion.minorVersion": project.versionMinor,
            "parsedVersion.incrementalVersion": project.versionBugfix,
            "buildNumber": project.buildNumber,
            "changeSet": mercurialInfo.changeSet,
            "platform": vms.platform,
            "arch": vms.arch,
            "modification": vms.modification,
            "ffmpeg.version": project.ffmpegVersion,
            "sigar.version": project.sigarVersion,
            "boost.version": project.boostVersion,
            "box": project.box || "none",
            "beta": project.beta,
            "product.name": customization.productName,
            "product.appName": customization.productNameShort,
            "product.name.short": customization.productNameShort,
            "display.product.name": customization.productDisplayName,
            "customization": project.customization,
            "translation1": customization.defaultTranslation,
            "client.binary.name": vms.clientBinaryName,
            "applauncher.binary.name": vms.applauncherBinaryName,
            "client.mediafolder.name": customization.productName + " Media",
            "company.license.address": customization.licensingEmail,
            "company.url": customization.companyUrl,
            "company.support.address": customization.supportEmail,
            "company.support.link": customization.supportUrl,
            "update.generator.url": customization.updateGeneratorUrl,
            "cloudHost": customization.cloudHost,
            "cloudName": customization.cloudName,
            "freeLicenseCount": customization.freeLicenseCount,
            "freeLicenseKey": customization.freeLicenseKey,
            "freeLicenseIsTrial": customization.freeLicenseIsTrial,
            "defaultWebPages": customization.defaultWebPages
        })
    }
    Group
    {
        condition: project.developerBuild
        files: "qt.conf"
        fileTags: "configure.input"
        configure.outputTags: "resources.resource_data"
        configure.outputProperties: ({
            "resources": {
                "priority": 10,
                "resourcePrefix": "qt/etc"
            }
        })
    }

    ResourcesGroup
    {
        resources.resourceSourceBase: product.sourceDirectory + "/static-resources"
    }
    ResourcesGroup
    {
        resources.priority: 1
        resources.resourceSourceBase: FileInfo.joinPaths(
            project.sourceDirectory,
            "customization",
            project.customization,
            "common",
            "resources")
    }

    Export
    {
        Depends { name: "cpp" }
        Depends
        {
            name: "Qt"
            submodules: [
                "network",
                "xml",
                "xmlpatterns",
                "sql",
                "concurrent",
                "multimedia"
            ]
        }
        Depends { name: "openssl" }
        Depends { name: "ffmpeg" }
        Depends
        {
            name: "quazip"
            condition: project.withDesktopClient || project.withMediaServer
        }
        Depends { name: "nx_fusion" }

        cpp.defines: ["NX_NETWORK_API=" + vms_cpp.apiImportMacro]
        cpp.includePaths: base.concat([project.sourceDirectory + "/common_libs/nx_network/src"])

        Properties
        {
            condition: qbs.targetOS.contains("android")
            cpp.dynamicLibraries: ["nx_streaming", "nx_network"]
        }
    }
}

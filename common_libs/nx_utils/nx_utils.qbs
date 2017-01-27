import qbs

GenericProduct
{
    name: "nx_utils"

    Depends { name: "Qt.core" }
    Depends { name: "boost" }
    Depends { name: "mercurialInfo" }

    cpp.defines: ["NX_UTILS_API=" + vms_cpp.apiExportMacro]

    Group
    {
        condition: qbs.targetOS.contains("ios")
        files: ["src/nx/utils/log/log_ios.mm"]
    }

    Group
    {
        files: "maven/filter-resources/app_info_impl.cpp"
        fileTags: "configure.input"
        configure.outputTags: "cpp"
        configure.replacements: ({
            "product.title": "", // TODO
            "product.display.title": "", // TODO
            "beta": project.beta,
            "parsedVersion.majorVersion": project.versionMajor,
            "parsedVersion.minorVersion": project.versionMinor,
            "parsedVersion.incrementalVersion": project.versionBugfix,
            "buildNumber": project.buildNumber,
            "changeSet": mercurialInfo.changeSet,
            "customization": project.customization,
            "platform": vms.platform,
            "arch": vms.arch,
            "box": project.box || "none"
        })
    }

    ResourcesGroup
    {
        resources.resourceSourceBase: product.sourceDirectory + "/static-resources"
    }

    Export
    {
        Depends { name: "cpp" }
        Depends { name: "Qt.core" }
        Depends { name: "boost" }

        cpp.defines: ["NX_UTILS_API=" + vms_cpp.apiImportMacro]
    }
}

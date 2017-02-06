import qbs
import qbs.FileInfo

GenericProduct
{
    name: "libclient"
    targetName: "client"

    condition: project.withDesktopClient

    Depends { name: "libclient_core" }
    Depends { name: "nx_speech_synthesizer" }
    Depends { name: "libvms_gateway" }

    Depends
    {
        condition: qbs.targetOS.contains("linux")
        name: "Qt.x11extras"
    }
    Depends
    {
        name: "Qt"
        submodules: [
            "widgets",
            "widgets-private",
            "opengl",
            "webkit",
            "webkitwidgets",
            "quickwidgets"
        ]
    }

    Depends
    {
        condition: qbs.targetOS.contains("windows")
        name: "openal"
    }
    Depends
    {
        condition: qbs.targetOS.contains("windows")
        name: "directx-JUN2010"
    }

    Properties
    {
        condition: qbs.targetOS.contains("linux")
        cpp.dynamicLibraries: base.concat(["GL", "Xfixes", "Xss", "X11"])
    }
    Properties
    {
        condition: qbs.targetOS.contains("macos")
        cpp.frameworks: ["Foundation", "AudioUnit", "AppKit"]
    }

    cpp.includePaths: ["src", project.sourceDirectory + "/cpp/maven/bin-resources"]

    Group
    {
        condition: qbs.targetOS.contains("macos")
        files: ["src/ui/workaround/mac_utils.mm"]
    }

    configure.replacements: ({
        "product.title": vms.clientName,
        "product.display.title": vms.clientDisplayName,
        "minilauncher.binary.name": vms.minilauncherBinaryName,
        "applauncher.binary.name": vms.applauncherBinaryName,
        "client.binary.name": vms.clientBinaryName,
        "installation.root": vms.installationRoot,
        "protocol_handler.app": vms.clientDisplayName,
        "mac.protocol_handler_bundle.identifier": customization.macBundleIdentifier,
        "launcher.version.file": vms.launcherVersionFile,
        "updateFeedUrl": vms.updateFeedUrl,
        "translation1": customization.defaultTranslation,
        "backgroundImage": ""
    })

    Group
    {
        files: "maven/filter-resources/client_app_info_impl.cpp"
        fileTags: "configure.input"
        configure.outputTags: "cpp"
    }
    Group
    {
        files: "maven/filter-resources/version.h"
        fileTags: "configure.input"
        configure.outputTags: "hpp"
    }
    Group
    {
        files: "maven/filter-resources/resources/globals.json"
        fileTags: "configure.input"
        configure.outputTags: "resources.resource_data"
    }

    Group
    {
        name: "forms"
        files: "*.ui"
        prefix: "src/**/"
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
            "libclient",
            "resources")
    }

    Export
    {
        Depends { name: "libclient_core" }
        Depends { name: "nx_speech_synthesizer" }
        Depends { name: "libvms_gateway" }

        Depends
        {
            condition: qbs.targetOS.contains("linux")
            name: "Qt.x11extras"
        }
        Depends
        {
            name: "Qt"
            submodules: [
                "widgets",
                "widgets-private",
                "opengl",
                "webkit",
                "webkitwidgets",
                "quickwidgets"
            ]
        }

        Properties
        {
            condition: qbs.targetOS.contains("linux")
            cpp.dynamicLibraries: base.concat(["GL", "Xfixes", "Xss", "X11"])
        }
    }
}

import qbs
import qbs.File

Product
{
    Depends { name: "cpp" }
    Depends { name: "vms" }
    Depends { name: "vms_cpp" }
    Depends { name: "resources" }
    Depends { name: "configure" }

    type: vms.defaultLibraryType

    cpp.includePaths: ["src"]
    cpp.warningLevel: "all"
    cpp.cxxLanguageVersion: "c++14"
    cpp.useCxxPrecompiledHeader: true
    cpp.positionIndependentCode: true
    cpp.useRPaths: true

    Properties
    {
        condition: qbs.targetOS.contains("android")
        qbs.optimization: qbs.buildVariant == "release" ? "fast" : "none"
    }
    Properties
    {
        condition: product.type.contains("application") && qbs.targetOS.contains("linux")
        overrideListProperties: !project.developerBuild // Leaving Qt RPATH for developer builds.
        cpp.rpaths: ["$ORIGIN/../lib"]
    }
    Properties
    {
        condition: qbs.targetOS.contains("macos")
        cpp.minimumMacosVersion: "10.8"
    }

    configure.outputTags: ["cpp", "hpp", "resources.resource_data"]

    Group
    {
        name: "sources"
        files: ["*.cpp", "*.h"]
        prefix: product.sourceDirectory + "/src/**/"
        excludeFiles: [
            "StdAfx.*",
            "*_win.*", "*_win/*",
            "*_unix.*", "*_unix/*",
            "*_linux.*", "*_linux/*",
            "*_mac.*", "*_mac/*",
            "*_macx.*", "*_macx/*",
            "*_android.*", "*_android/*",
            "*_ios.*", "*_ios/*"
        ]
    }

    Group
    {
        name: "precompiled header"
        files: product.sourceDirectory + "/src/StdAfx.h"
        fileTags: "cpp_pch_src"
    }

    Group
    {
        name: "win"
        files: ["*_win.cpp", "*_win.h", "*_win/*.cpp", "*_win/*.h"]
        prefix: product.sourceDirectory + "/src/**/"
        condition: qbs.targetOS.contains("windows")
    }

    Group
    {
        name: "unix"
        files: ["*_unix.cpp", "*_unix.h", "*_unix/*.cpp", "*_unix/*.h"]
        prefix: product.sourceDirectory + "/src/**/"
        condition: qbs.targetOS.contains("unix")
    }

    Group
    {
        name: "linux"
        files: ["*_linux.cpp", "*_linux.h", "*_linux/*.cpp", "*_linux/*.h"]
        prefix: product.sourceDirectory + "/src/**/"
        condition: qbs.targetOS.contains("linux")
    }

    Group
    {
        name: "mac"
        files: ["*_mac.cpp", "*_mac.h", "*_mac/*.cpp", "*_mac/*.h"]
        prefix: product.sourceDirectory + "/src/**/"
        condition: qbs.targetOS.contains("darwin")
    }

    Group
    {
        name: "macx"
        files: ["*_macx.cpp", "*_macx.h", "*_macx/*.cpp", "*_macx/*.h"]
        prefix: product.sourceDirectory + "/src/**/"
        condition: qbs.targetOS.contains("macos")
    }

    Group
    {
        name: "ios"
        files: ["*_ios.cpp", "*_ios.h", "*_ios/*.cpp", "*_ios/*.h"]
        prefix: product.sourceDirectory + "/src/**/"
        condition: qbs.targetOS.contains("ios")
    }

    Group
    {
        name: "android"
        files: ["*_android.cpp", "*_android.h", "*_android/*.cpp", "*_android/*.h"]
        prefix: product.sourceDirectory + "/src/**/"
        condition: qbs.targetOS.contains("android")
    }

    Group
    {
        name: "translations"
        files: ["*.ts"]
        prefix: product.sourceDirectory + "/translations/"
    }
    Rule
    {
        inputs: ["qm"]

        Artifact
        {
            filePath: "translations/" + input.fileName
            fileTags: ["resources.resource_data"]
            resources.resourcePrefix: "translations"
        }
        prepare:
        {
            var cmd = new JavaScriptCommand()
            cmd.silent = true
            cmd.sourceCode = function() { File.copy(input.filePath, output.filePath) }
            return cmd
        }
    }

    Export
    {
        Depends { name: "cpp" }
        Depends { name: "vms" }
        Depends { name: "vms_cpp" }
        cpp.includePaths: [product.sourceDirectory + "/src"]
    }

    Group
    {
        fileTagsFilter: product.type
        qbs.install: !product.type.contains("staticlibrary")
        qbs.installDir: {
            if (product.type.contains("dynamiclibrary"))
                return "lib"
            if (product.type.contains("application"))
                return "bin"
            return ""
        }
    }
}

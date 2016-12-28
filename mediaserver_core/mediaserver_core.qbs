import qbs
import qbs.File
import qbs.FileInfo
import qbs.Utilities

Project
{
    condition: withMediaServer

    RdepProbe
    {
        id: onvif
        packageName: "onvif"
    }

    RdepProbe
    {
        id: sigar
        packageName: "sigar"
    }

    RdepProbe
    {
        id: nxSdk
        packageName: "nx_sdk-1.6.0"
        target: "any"
    }

    RdepProbe
    {
        id: nxStorageSdk
        packageName: "nx_storage_sdk-1.6.0"
        target: "any"
    }

    references: [
        onvif.packagePath,
        sigar.packagePath,
        nxSdk.packagePath,
        nxStorageSdk.packagePath
    ]

    GenericProduct
    {
        name: "mediaserver_core"

        Depends { name: "qtservice" }
        Depends { name: "qtsinglecoreapplication" }
        Depends { name: "onvif" }
        Depends { name: "sigar" }
        Depends { name: "nx_sdk" }
        Depends { name: "nx_storage_sdk" }

        Depends { name: "appserver2" }
        Depends { name: "nx_email" }
        Depends { name: "nx_speech_synthesizer" }
        Depends { name: "cloud_db_client" }

        Properties
        {
            condition: qbs.targetOS.contains("linux")
            cpp.dynamicLibraries: ["ldap"]
        }

        Group
        {
            files: "maven/filter-resources/mediaserver_core_app_info_impl.cpp"
            fileTags: "configure.input"
            configure.outputTags: "cpp"
            configure.replacements: ({
                "product.title": customization.companyName + " Media Server",
                "product.display.title": customization.productDisplayName + " Media Server"
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
                "mediaserver_core",
                "resources")
        }

        Rule
        {
            inputsFromDependencies: ["sdk_package"]
            Artifact
            {
                filePath: FileInfo.joinPaths(Utilities.getHash(input.baseDir), input.fileName)
                fileTags: ["resources.resource_data"]
                resources.resourcePrefix: "static"
            }
            prepare: {
                cmd = new JavaScriptCommand()
                cmd.silent = true
                cmd.sourceCode = function() { File.copy(input.filePath, output.filePath) }
                return cmd
            }
        }
        Group
        {
            files: "api/api_template.xml"
            fileTags: ["api_template"]
        }
        Rule
        {
            inputs: ["api_template"]
            Artifact
            {
                filePath: "api.xml"
                fileTags: ["resources.resource_data"]
                resources.resourcePrefix: "static"
            }
            prepare: {
                var args = [
                    "-jar",
                    FileInfo.joinPaths(project.buildenvDirectory, "bin/apidoctool.jar"),
                    "code-to-xml",
                    "-vms-path",
                    project.sourceDirectory,
                    "-template-xml",
                    input.filePath,
                    "-output-xml",
                    output.filePath]

                cmd = new Command("java", args)
                cmd.description = "generating " + input.fileName
                cmd.highlight = "filegen"
                return cmd
            }
        }

        Export
        {
            Depends { name: "qtservice" }
            Depends { name: "qtsinglecoreapplication" }
            Depends { name: "onvif" }
            Depends { name: "sigar" }
            Depends { name: "appserver2" }
            Depends { name: "nx_email" }
            Depends { name: "nx_speech_synthesizer" }
            Depends { name: "cloud_db_client" }
        }
    }
}

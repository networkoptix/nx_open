import qbs
import qbs.ModUtils
import qbs.File
import qbs.FileInfo

Product
{
    name: "client-deb"
    type: ["deb_package"]

    targetName: [
        customization.installerName,
        "client",
        project.fullVersion,
        vms.arch,
        qbs.buildVariant].join("-")
        + (project.beta ? "-beta" : "")
        + ".deb"

    Depends { name: "vms" }
    Depends { name: "customization" }
    Depends { name: "desktop_client" }

    Rule
    {
        multiplex: true
        inputsFromDependencies: ["installable"]

        Artifact
        {
            filePath: product.targetName
            fileTags: product.type
        }

        prepare:
        {
            var cmd = new JavaScriptCommand()
            cmd.description = "building" + output.fileName
            cmd.highlight = "linker"
            cmd.sourceCode = function()
            {
                for (var i = 0; i < inputs["installable"].length; ++i)
                {
                    var inp = inputs["installable"][i]
                    var installRoot = inp.moduleProperty("qbs", "installRoot")
                    var installedFilePath = ModUtils.artifactInstalledFilePath(inp)
                    console.info(">>" + FileInfo.relativePath(installRoot, installedFilePath))
                }

            }
            return [cmd]
        }
    }
}

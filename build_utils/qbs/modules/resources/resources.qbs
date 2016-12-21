import qbs
import qbs.TextFile
import qbs.FileInfo

Module
{
    property string resourcePrefix
    property string resourceSourceBase
    property int priority: 0

    Rule
    {
        multiplex: true

        inputs: ["resources.resource_data"]

        Artifact
        {
            filePath: product.name + ".qrc"
            fileTags: ["qrc"]
        }

        prepare:
        {
            var cmd = new JavaScriptCommand()
            cmd.description = "generating " + output.fileName
            cmd.highlight = "codegen"
            cmd.sourceCode = function()
            {
                var map = {}
                for (var i = 0; i < inputs["resources.resource_data"].length; ++i)
                {
                    var input = inputs["resources.resource_data"][i]
                    var prefix = input.moduleProperty("resources", "resourcePrefix") || ""
                    var basePath = input.moduleProperty("resources", "resourceSourceBase") || ""
                    var priority = input.moduleProperty("resources", "priority") || 0

                    var alias = basePath
                        ? FileInfo.relativePath(basePath, input.filePath)
                        : input.fileName
                    if (prefix)
                        alias = FileInfo.joinPaths(prefix, alias)

                    var entry = map[alias]
                    if (entry && entry.priority <= priority)
                        continue

                    map[alias] = { "path": input.filePath, "priority": priority }
                }

                var qrcFile = new TextFile(output.filePath, TextFile.WriteOnly)
                qrcFile.writeLine("<!DOCTYPE RCC>")
                qrcFile.writeLine("<RCC version=\"1.0\">")
                qrcFile.writeLine("<qresource prefix=\"/\">")

                for (var alias in map)
                    qrcFile.writeLine("<file alias=\"" + alias + "\">" + map[alias].path + "</file>")

                qrcFile.writeLine("</qresource>")
                qrcFile.writeLine("</RCC>")
                qrcFile.close()
            }
            return cmd
        }
    }
}

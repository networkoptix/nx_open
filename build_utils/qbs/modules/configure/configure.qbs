import qbs
import qbs.Utilities
import qbs.FileInfo
import "configure.js" as Configure

Module {
    property stringList outputTags: ["configure.output"]
    property var outputProperties
    property var replacements
    property bool ignoreUndefinedValues: false
    property var variableRegExp: /\$\{(.*?)\}|@(.*?)@/

    Rule {
        inputs: ["configure.input"]

        outputFileTags: product.moduleProperty("configure", "outputTags")
        outputArtifacts: {
            var artifact = input.moduleProperty("configure", "outputProperties");
            if (!artifact)
                artifact = {};

            artifact.filePath = FileInfo.joinPaths(
                Utilities.getHash(input.baseDir), input.fileName);
            artifact.fileTags = input.moduleProperty("configure", "outputTags");

            return [artifact];
        }

        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "generating " + output.fileName;
            cmd.highlight = "codegen";
            cmd.sourceCode = function() { Configure.configureFile(input, output); };
            return [cmd];
        }
    }
}

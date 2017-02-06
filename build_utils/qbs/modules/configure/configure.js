var TextFile = loadExtension("qbs.TextFile");

function configureFile(input, output) {
    var replacements = input.moduleProperty("configure", "replacements");
    var ignoreUndefinedValues = input.moduleProperty("configure", "ignoreUndefinedValues");

    var variableRegExp = input.moduleProperty("configure", "variableRegExp");
    variableRegExp = new RegExp(variableRegExp.source, "g");

    var formatMessage = function(file, line, pos, message) {
        return file + ":" + line + ":" + (pos + 1) + ": " + message;
    };

    var getValue = function(variableString) {
        // Replacements map always has priority over item properties.
        if (replacements && variableString in replacements) {
            return replacements[variableString];
        } else {
            // First check if the item is project, product, input or output.
            // That's why the first index of '.' is used.
            var i = variableString.indexOf(".");
            if (i !== -1) {
                var module = variableString.slice(0, i);
                var variable = variableString.slice(i + 1);
                var object = input;

                if (module && variable) {
                    if (module === "product" || module === "project"
                        || module === "input" || module === "output")
                    {
                        object = eval(module);
                        module = undefined;
                        variableString = variable;
                    }

                    // Second re-split variable name using last index of '.'.
                    // This handles cases of compound module names like "Qt.core".
                    i = variableString.lastIndexOf(".");
                    if (i !== -1) {
                        module = variableString.slice(0, i);
                        variable = variableString.slice(i + 1);
                    }

                    if (module && variable)
                        return object.moduleProperty(module, variable);
                    else
                        return object[variableString];
                }
            }
        }
    };

    var inFile;
    var outFile;

    try {
        inFile = new TextFile(input.filePath);
        outFile = new TextFile(output.filePath, TextFile.WriteOnly);
        outFile.truncate();

        var lineNumber = 1;
        while (!inFile.atEof()) {
            var line = inFile.readLine();

            var replaceFunction = function(str) {
                if (arguments.length < 4)
                    throw "variableRegExp does not seem to contain capture group.";

                var variableString;
                for (var i = 1; i < arguments.length; ++i) {
                    variableString = arguments[i];
                    if (variableString)
                        break;
                }

                if (!variableString)
                    return str;

                var value = getValue(variableString);
                if (value === undefined) {
                    var pos = arguments[arguments.length - 2];
                    var message = formatMessage(
                        input.filePath, lineNumber, pos, "Undefined value " + str);

                    if (ignoreUndefinedValues) {
                        console.warn(message);
                        return str;
                    } else {
                        throw message;
                    }
                }

                return value.toString();
            }

            outFile.writeLine(line.replace(variableRegExp, replaceFunction));
            ++lineNumber;
        }
    } finally {
        inFile.close();
        outFile.close();
    }
}

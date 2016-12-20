/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing
**
** This file is part of the Qt Build Suite.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms and
** conditions see http://www.qt.io/terms-conditions. For further information
** use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file.  Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, The Qt Company gives you certain additional
** rights.  These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

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
        if (variableString in replacements) {
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

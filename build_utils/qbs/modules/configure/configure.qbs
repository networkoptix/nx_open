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

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

.pragma library

function attributeValuesFromFilters(stringList)
{
    // This simple regular expression is intended to parse back a generated attribute string.
    // To parse any valid user input it should be further improved.
    //
    // Currently it matches:
    //    any number of whitespaces
    //         -FOLLOWED BY-
    //
    //    any one or more characters between quotation marks, except !, =, : or "
    //                            -OR-
    //    any one or more characters, except !, =, :, " or a whitespace
    //
    //         -FOLLOWED BY-
    //    zero or more whitespaces
    //         -FOLLOWED BY-
    //    optional !
    //         -FOLLOWED BY-
    //    = or :
    //         -FOLLOWED BY-
    //    zero or more whitespaces
    //         -FOLLOWED BY-
    //
    //    any one or more characters between quotation marks, except "
    //                            -OR-
    //    any one or more non-whitespace characters
    //
    //         -FOLLOWED BY-
    //    any number of whitespaces

    const regex = /\s*("[^!=:"]+"|[^"!=:\s]+)\s*(!*=|:)\s*("[^"]+"|\S+)\s*$/
    let attributeTree = {}

    for (let i = 0; i < stringList.length; ++i)
    {
        const match = regex.exec(stringList[i])
        if (!match)
        {
            console.warn(loggingCategory, `Malformed filter: ${stringList[i]}`)
            continue
        }

        const unquote =
            (str) => str[0] === "\"" ? str.substr(1, str.length - 2) : str
        const parseBoolean =
            (str) => str === "true" || str === "false" ? str === "true" : undefined

        const fullName = unquote(match[1])
        const path = fullName.split(".")
        let value = parseBoolean(match[3]) ?? unquote(match[3])
        const operation = match[2]

        if (operation === "!=")
        {
            // Currently supported only "!= true" for object values.
            const booleanTrue = /true/i.test(value)
            if (!booleanTrue)
            {
                console.warn(loggingCategory, `Unsupported filter: ${match[0]}`)
                continue
            }

            // Internally we replace "!= true" logic to "== false".
            value = false
        }

        let obj = attributeTree
        while (path.length > 1)
        {
            const name = path.shift()
            if (typeof obj[name] !== "object")
                obj[name] = {}

            obj = obj[name]
        }

        obj[path[0]] = value
    }

    return attributeTree
}

function attributeFiltersFromValues(values, parentName)
{
    function quoteIfNeeded(text, quoteNumbers)
    {
        if (text.search(/\s/) >= 0)
            return `"${text}"`

        return (quoteNumbers && text.match(/^[+-]?\d+(\.\d+)?$/))
            ? `"${text}"`
            : text
    }

    var result = []
    for (let name in values)
    {
        const value = values[name]
        const fullName = parentName
            ? `${parentName}.${name}`
            : name
        if (typeof value === "object")
            result = result.concat(attributeFiltersFromValues(value, name))
        else if (typeof value === "function")
            result.push(value(quoteIfNeeded(fullName)))
        else
            result.push(`${quoteIfNeeded(fullName)}=${quoteIfNeeded(value.toString(), true)}`)
    }

    return result
}

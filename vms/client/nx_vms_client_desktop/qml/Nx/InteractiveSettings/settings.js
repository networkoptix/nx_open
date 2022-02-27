// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

function _nameSectionsRecursively(parentSection, parentSectionId)
{
    let captionOccurences = {}
    if (!parentSection.sections)
        return

    for (let i = 0; i < parentSection.sections.length; ++i)
    {
        let section = parentSection.sections[i]

        if (section.name)
            section.name = "name:" + section.name

        let occurences = captionOccurences[section.caption]
        if (!occurences)
            occurences = 0

        section.name = parentSectionId + ".capt" + (occurences || "") + ":" + section.caption
        captionOccurences[section.caption] = occurences + 1

        _nameSectionsRecursively(section, section.name)
    }
}

function preprocessModel(model)
{
    model.type = "Settings"
    _nameSectionsRecursively(model, "")
}

function _createItemsRecursively(parent, visualParent, model, depth)
{
    var type = model.type
    if (!type)
        return null

    if (type === "GroupBox" && depth === 1)
        type = "Panel"

    const componentPath = "components/%1.qml".arg(type)
    const component = Qt.createComponent(componentPath)
    if (!component)
    {
        console.error("Cannot create component %1".arg(componentPath))
        return null
    }

    var item = component.createObject(visualParent, model)

    if (item)
    {
        if (item.valueChanged)
            item.valueChanged.connect(settingsView.triggerValuesEdited)

        if (item.filledChanged && parent.processFilledChanged)
        {
            item.filledChanged.connect(function() { parent.processFilledChanged(item) })
            parent.processFilledChanged(item)
        }

        if (item.childrenItem && model.items)
        {
            model.items.forEach(
                function(model)
                {
                    _createItemsRecursively(item, item.childrenItem, model, depth + 1)
                })
        }

        if (item.sectionStack && model.sections)
        {
            model.sections.forEach(
                function(model)
                {
                    _createItemsRecursively(item, item.sectionStack, model, depth)
                })
        }
    }
    else
    {
        if (component.status === Component.Error)
            console.error(component.errorString())
    }

    return item
}

function createItems(parent, model)
{
    let item = _createItemsRecursively(parent, parent.childrenItem, model, 0)
    if (item)
    {
        item.parent = parent
        item.anchors.fill = parent
    }
    return item
}

function _processSectionsRecursively(section, path, f)
{
    f(section, path)

    if (section.sections)
    {
        for (let i = 0; i < section.sections.length; ++i)
        {
            const current = section.sections[i]
            _processSectionsRecursively(current, path.concat(i), f)
        }
    }
}

function buildSectionPaths(model)
{
    let sectionPaths = {}
    _processSectionsRecursively(model, [],
        function(section, path) { sectionPaths[section.name] = path })
    return sectionPaths
}

function sectionNameByPath(model, path)
{
    if (!model || !model.sections || !path)
        return undefined

    const index = path[0]
    if (model.sections.length <= index)
        return undefined

    return sectionNameByPath(model.sections[index], path.slice(1))
}

function _processItemsRecursively(item, f)
{
    f(item)

    if (item.childrenItem)
    {
        for (var i = 0; i < item.childrenItem.children.length; ++i)
            _processItemsRecursively(item.childrenItem.children[i], f)
    }

    if (item.sectionStack)
    {
        for (var i = 1; i < item.sectionStack.children.length; ++i)
            _processItemsRecursively(item.sectionStack.children[i], f)
    }
}

function getValues(rootItem)
{
    var result = {}

    _processItemsRecursively(rootItem,
        function(item)
        {
            if (item.getValue !== undefined)
            {
                try
                {
                    result[item.name] = item.getValue()
                }
                catch (error)
                {
                    console.warn("getValue() failed for item %1:".arg(item.name), error)
                }
            }
            else if (item.hasOwnProperty("value"))
            {
                result[item.name] = item.value
            }
        })

    return result
}

function setValues(rootItem, values)
{
    _processItemsRecursively(rootItem,
        function(item)
        {
            if (values.hasOwnProperty(item.name))
            {
                if (item.setValue !== undefined)
                    item.setValue(values[item.name])
                else if (item.hasOwnProperty("value"))
                    item.value = values[item.name]
            }
            else
            {
                if (item.resetValue !== undefined)
                    item.resetValue()
                else if (item.hasOwnProperty("value") && item.hasOwnProperty("defaultValue"))
                    item.value = item.defaultValue
            }
        })
}

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

function _connectSignals(parent, item)
{
    if (item.activeValueEdited)
    {
        item.activeValueEdited.connect(
            () => settingsView.triggerValuesEdited(item.isActive ? item : null))
    }

    if (item.valueChanged)
    {
        // An item can be active and only have a `valueChanged` signal.
        let isDedicatedActiveSignalUsed = item.activeValueEdited
        let activeItem =
            (item.isActive && !isDedicatedActiveSignalUsed) ? item : null

        item.valueChanged.connect(() => settingsView.triggerValuesEdited(activeItem))
    }

    if (item.activated)
        item.activated.connect(() => settingsView.triggerValuesEdited(item))

    if (item.filledChanged && parent.processFilledChanged)
    {
        item.filledChanged.connect(function() { parent.processFilledChanged(item) })
        parent.processFilledChanged(item)
    }
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
        _connectSignals(parent, item)

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

    if (!item)
        item = _createItemsRecursively(parent, parent.childrenItem, { "type": "Settings" }, 0)

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

function setValue(item, value)
{
    if (item.setValue !== undefined)
        item.setValue(value)
    else if (item.hasOwnProperty("value"))
        item.value = value
}

function resetValue(item)
{
    if (item.resetValue !== undefined)
        item.resetValue()
    else
        setValue(item, item.hasOwnProperty("defaultValue") ? item.defaultValue : null)
}

function setValues(rootItem, values)
{
    _processItemsRecursively(rootItem,
        function(item)
        {
            if (values.hasOwnProperty(item.name))
                setValue(item, values[item.name])
            else
                resetValue(item)
        })
}

function resetValues(rootItem)
{
    _processItemsRecursively(rootItem, resetValue)
}

function setErrors(rootItem, errors)
{
    _processItemsRecursively(rootItem,
        (item) =>
        {
            if (item.name && errors[item.name] && item.setErrorMessage !== undefined)
                item.setErrorMessage(errors[item.name])
        })
}

function takeFocusState(rootItem)
{
    let state = {}

    _processItemsRecursively(rootItem,
        (item) =>
        {
            if (item.activeFocus && item.getFocusState && item.name)
            {
                state[item.name] = item.getFocusState()
                item.focus = false
            }
        })

    return state
}

function setFocusState(rootItem, state)
{
    _processItemsRecursively(rootItem,
        (item) =>
        {
            if (item.setFocusState && (item.name in state))
            {
                item.setFocusState(state[item.name])
                item.forceActiveFocus()
            }
        })
}

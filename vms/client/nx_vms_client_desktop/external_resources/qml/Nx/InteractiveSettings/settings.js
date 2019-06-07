function _createItemsRecursively(parent, model, depth)
{
    var type = model.type
    if (type === "GroupBox" && depth === 1)
        type = "Panel"

    const componentPath = "components/%1.qml".arg(type)
    const component = Qt.createComponent(componentPath)
    if (!component)
    {
        console.error("Cannot create component %1".arg(componentPath))
        return null
    }

    var item = component.createObject(parent.childrenItem, model)

    if (item)
    {
        if (item.valueChanged)
            item.valueChanged.connect(settingsView.triggerValuesEdited)

        if (item.childrenItem && model.items)
        {
            model.items.forEach(
                function(model) { _createItemsRecursively(item, model, depth + 1) })
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
    // Clone the model because we are going to modify it.
    var modelCopy = JSON.parse(JSON.stringify(model))
    modelCopy.type = "Settings"

    var item = _createItemsRecursively(parent, modelCopy, 0)
    item.parent = parent
    item.anchors.fill = parent

    return item
}

function _processItemsRecursively(item, f)
{
    f(item)

    if (item.childrenItem)
    {
        for (var i = 0; i < item.childrenItem.children.length; ++i)
            _processItemsRecursively(item.childrenItem.children[i], f)
    }
}

function getValues(rootItem)
{
    var result = {}

    _processItemsRecursively(rootItem,
        function(item)
        {
            if (item.getValue !== undefined)
                result[item.name] = item.getValue()
            else if (item.hasOwnProperty("value"))
                result[item.name] = item.value
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

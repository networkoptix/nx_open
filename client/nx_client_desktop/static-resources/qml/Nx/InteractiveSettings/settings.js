function _createItemsRecursively(parent, model, depth)
{
    var type = model.type
    if (type === "GroupBox" && depth === 1)
        type = "Panel"

    const component = Qt.createComponent("components/%1.qml".arg(type))
    if (!component)
        return null

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
    if (item.hasOwnProperty("value"))
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

    _processItemsRecursively(rootItem, function(item) { result[item.name] = item.value })

    return result
}

function setValues(rootItem, values)
{
    _processItemsRecursively(rootItem,
        function(item)
        {
            if (item.hasOwnProperty("value"))
            {
                if (values.hasOwnProperty(item.name))
                    item.value = values[item.name]
                else
                    item.value = item.defaultValue
            }
        })
}

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

export function pollButtons(newState)
{
    if (this.previousState === null)
        this.previousState = newState

    for (let i = 0; i < newState.length; ++i)
    {
        if (newState[i] !== this.previousState[i])
        {
            const buttonRow = this.buttonRows.find(button => button.bitNumber === i)

            if (buttonRow === undefined)
            {
                const newButton =
                    {
                        bitNumber: i,
                        name: `Button ${i}`,
                        value: newState[i] ? 1 : 0,
                    }

                this.buttonRows = [ ...this.buttonRows, newButton ]
            }
            else
            {
                let newButtonRows = this.buttonRows.slice()
                const changedButtonIndex = newButtonRows.findIndex(button => button.bitNumber === i)
                newButtonRows[changedButtonIndex].value = newState[i] ? 1 : 0
                this.buttonRows = newButtonRows
            }
        }
    }

    this.previousState = newState
}

export function getButtonsCollectedData()
{
    return this.buttonRows.reduce(
        (buttonsData, buttonRow) =>
        {
            buttonsData[buttonRow.bitNumber] = buttonRow.name
            return buttonsData
        },
        {}
    )
}

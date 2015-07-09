import QtQuick 2.1;

import "../base" as Base;


Base.TextField
{
    id: thisComponent;

    property string defaultEmptyValue: "0.0.0.0";
    
    validator: RegExpValidator
    {
        regExp: /^(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$/;
    }

    text: initialText;

    onEnabledChanged:
    {
        if (!enabled && !acceptableInput)
            text = initialText;
    }

    onTextChanged: 
    {
        //maximum value of the first two digits is 25, otherwise appending dot
        var maximumOctetIntermediateValue = 25;

        var octet = text.substring(text.lastIndexOf('.') + 1, text.length);       
        // check if we can append any symbol - or already have valid ip
        if (octet.length < 2 || acceptableInput)
            return;
        
        if (parseInt(octet) > maximumOctetIntermediateValue)
            text = text + '.';
    }
}


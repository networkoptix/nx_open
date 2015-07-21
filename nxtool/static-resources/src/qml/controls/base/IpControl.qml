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
}


import QtQuick 2.6

Grid
{

    columns: 3
    rows: 3
    spacing: 0

    Repeater
    {
        model: 9

        Button
        {
            width: 50
            height: 50

            text:
            {
                switch(index)
                {
                    case 1:
                        return "^"
                    case 3:
                        return "<"
                    case 5:
                        return ">"
                    case 7:
                        return "v"
                }
                return "";
            }

            onClicked:
            {
                switch(index)
                {
                    case 1:
                        console.log("-- move up");
                        break;
                    case 3:
                        console.log("-- move left");
                        break;
                    case 4:
                        console.log("-- stop");
                        break;
                    case 5:
                        console.log("-- move right")
                        break;
                    case 7:
                        console.log("-- move down")
                        break;
                }
            }
        }
    }
}

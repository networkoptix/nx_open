import "private"

Figure
{
    figureType: "line"

    property string allowedDirections: ""
    property int minPoints: 2
    property int maxPoints: 0

    figureSettings:
    {
        "minPoints": minPoints,
        "maxPoints": maxPoints,
        "allowedDirections": allowedDirections
    }
}

import "private"

Figure
{
    figureType: "line"

    property string allowedDirections: ""
    property int maxPoints: 0

    figureSettings:
    {
        "maxPoints": maxPoints,
        "allowedDirections": allowedDirections
    }
}

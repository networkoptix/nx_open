var SphereProjectionTypes = 
{ 
    Equidistant: 0, 
    Stereographic: 1,
    Equisolid: 2 
}

function radians(degrees)
{
    return degrees * Math.PI / 180.0
}

function degrees(radians)
{
    return radians / Math.PI * 180.0
}

function rotationX(radians)
{
    var sin = Math.sin(radians)
    var cos = Math.cos(radians)

    return Qt.matrix4x4(
           1,    0,    0,    0,
           0,  cos,  sin,    0,
           0, -sin,  cos,    0,
           0,    0,    0,    1);
}

function rotationY(radians)
{
    var sin = Math.sin(radians)
    var cos = Math.cos(radians)

    return Qt.matrix4x4(
         cos,    0,  sin,    0,
           0,    1,    0,    0,
        -sin,    0,  cos,    0,
           0,    0,    0,    1);
}

function rotationZ(radians)
{
    var sin = Math.sin(radians)
    var cos = Math.cos(radians)

    return Qt.matrix4x4(
         cos, -sin,    0,    0,
         sin,  cos,    0,    0,
           0,    0,    1,    0,
           0,    0,    0,    1);
}

function scaling(sx, sy, sz)
{
    return Qt.matrix4x4(
          sx,    0,    0,    0,
           0,   sy,    0,    0,
           0,    0,   sz,    0,
           0,    0,    0,    1);
}

function uniformScaling(s)
{
    return scaling(s, s, s)
}

function translation(dx, dy, dz)
{
    return Qt.matrix4x4(
           1,    0,    0,   dx,
           0,    1,    0,   dy,
           0,    0,    1,   dz,
           0,    0,    0,    1);
}

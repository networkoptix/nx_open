function dp(dpix) {
    return Math.round(dpix * screenPixelMultiplier)
}

function sp(dpix) {
    return dp(dpix)
}

function interpolate(x1, x2, p) {
    return x1 + (x2 - x1) * p
}

function perc(x1, x2, x) {
    return (x - x1) / (x2 - x1)
}

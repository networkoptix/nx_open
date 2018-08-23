import QtQuick 2.0

QtObject
{
    property color base1:       "#000000"
    property color base2:       "#080707"
    property color base3:       "#0d0e0f"
    property color base4:       "#121517"
    property color base5:       "#171c1f"
    property color base6:       "#1c2327"
    property color base7:       "#212a2f"
    property color base8:       "#263137"
    property color base9:       "#2b383f"
    property color base10:      "#303f47"
    property color base11:      "#35464f"
    property color base12:      "#3a4d57"
    property color base13:      "#3f545f"
    property color base14:      "#445b67"
    property color base15:      "#49626f"
    property color base16:      "#4e6977"
    property color base17:      "#53707f"

    property color contrast1:   "#ffffff"
    property color contrast2:   "#f5f7f8"
    property color contrast3:   "#ebeff1"
    property color contrast4:   "#e1e7ea"
    property color contrast5:   "#d7dfe3"
    property color contrast6:   "#cdd7dc"
    property color contrast7:   "#c3cfd5"
    property color contrast8:   "#b9c7ce"
    property color contrast9:   "#afbfc7"
    property color contrast10:  "#a5b7c0"
    property color contrast11:  "#9bafb9"
    property color contrast12:  "#91a7b2"
    property color contrast13:  "#879fab"
    property color contrast14:  "#7d97a4"
    property color contrast15:  "#738f9d"
    property color contrast16:  "#698796"
    property color contrast17:  "#5f7f8f"

    property color blue_l2:     "#43c2ff"
    property color blue_l1:     "#39b2ef"
    property color blue_main:   "#2fa2db"
    property color blue_d1:     "#2592c3"
    property color blue_d2:     "#1b82ad"
    property color blue_d3:     "#117297"
    property color blue_d4:     "#076281"

    property color brand_l4:    "#77d2ff"
    property color brand_l3:    "#5ecbff"
    property color brand_l2:    "#43c2ff"
    property color brand_l1:    "#39b2ef"
    property color brand_main:  "#2fa2db"
    property color brand_d1:    "#2592c3"
    property color brand_d2:    "#1b82ad"
    property color brand_d3:    "#117297"
    property color brand_d4:    "#076281"
    property color brand_d5:    "#045773"
    property color brand_d6:    "#054a61"
    property color brand_d7:    "#043e51"

    property color brand_contrast: "#ffffff"

    property color orange_main: "#d65029"
    property color orange_d1:   "#743320"

    property color green_l4:    "#66ef52"
    property color green_l3:    "#59dc2e"
    property color green_l2:    "#4fc527"
    property color green_l1:    "#41aa22"
    property color green_main:  "#3a911e"
    property color green_d1:    "#32731e"
    property color green_d2:    "#2a551e"
    property color green_d3:    "#223925"

    property color red_l2:      "#e34545"
    property color red_l1:      "#d53232"
    property color red_main:    "#c22626"
    property color red_d1:      "#aa1e1e"
    property color red_d2:      "#8e1717"
    property color red_d3:      "#741414"
    property color red_d4:      "#5e1010"

    property color yellow_main: "#fbbc05"
    property color yellow_d1:   "#e0a30a"
    property color yellow_d2:   "#c68a0f"

    property color windowBackground: base5
    property color windowText: contrast1
    property color brightText: contrast1
    property color highlight: brand_main

    property color backgroundDimColor: transparent(base5, 0.4)

    function transparent(color, alpha)
    {
        return Qt.rgba(color.r, color.g, color.b, alpha)
    }
}

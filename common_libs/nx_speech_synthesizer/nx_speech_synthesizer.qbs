import qbs

GenericProduct
{
    name: "nx_speech_synthesizer"

    Depends { name: "common" }
    Depends { name: "festival-headers"; condition: !qbs.targetOS.contains("macos") }
    Depends { name: "festival"; condition: qbs.targetOS.contains("macos") }

    cpp.allowUnresolvedSymbols: true

    Export
    {
        Depends { name: "common" }
        Depends { name: "festival-headers"; condition: !qbs.targetOS.contains("macos") }
        Depends { name: "festival"; condition: qbs.targetOS.contains("macos") }
    }
}

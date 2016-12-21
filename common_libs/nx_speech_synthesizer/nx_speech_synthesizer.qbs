import qbs

GenericProduct
{
    name: "nx_speech_synthesizer"

    Depends { name: "common" }
    Depends { name: "festival-headers" }

    cpp.allowUnresolvedSymbols: true

    Export
    {
        Depends { name: "common" }
        Depends { name: "festival-headers" }
    }
}

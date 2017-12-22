#include "json_data.h"

namespace nx {
namespace update {
namespace info {
namespace detail {
namespace data_provider {
namespace test_support {

const QByteArray& metaDataJson()
{
    static const QByteArray result = R"JSON(
{
    "__info": [
        {
            "url": "http://beta.networkoptix.com/beta-builds/daily/updates.json",
            "name": "mono-us"
        }
    ],
    "win4net": {
        "release_notes": "http://updates.hdwitness.com/release_notes.html",
        "updates_prefix": "http://updates.networkoptix.com/win4net",
        "releases": {
            "2.3": "2.3.2.9167",
            "2.4": "2.4.0.0",
            "2.5": "2.5.0.11376",
            "2.2": "2.2.1.3217"
        }
    },
    "nnodal": {
        "release_notes": "http://updates.hdwitness.com/release_notes.html",
        "updates_prefix": "http://updates.networkoptix.com/nnodal",
        "releases": {
            "2.3": "2.3.2.9167",
            "2.4": "2.4.1.10278",
            "2.2": "2.2.1.3217",
            "2.5": "2.5.0.11500",
            "2.6": "2.6.0.13109"
        }
    },
    "airylife": {
        "release_notes": "http://updates.hdwitness.com/release_notes.html",
        "updates_prefix": "http://updates.networkoptix.com/airylife",
        "releases": {
            "2.3": "2.3.0.8344",
            "2.4": "2.4.0.0",
            "2.2": "2.2.1.3217"
        }
    },
    "vmsdemoorange": {
        "release_notes": "http://www.networkoptix.com/all-nx-witness-release-notes",
        "updates_prefix": "http://updates.networkoptix.com/vmsdemoorange",
        "releases": {
            "2.2": "2.2.1.3217",
            "2.3": "2.3.2.9167",
            "2.4": "2.4.1.10278",
            "3.0": "3.0.0.15297",
            "2.5": "2.5.0.11500",
            "2.6": "2.6.0.13109",
            "3.1": "3.1.0.16975"
        },
        "download_version": "3.1.0.16975",
        "current_release": "3.1",
        "description": "",
        "release_date": "1510776791891",
        "release_delivery": 30
    },
    "digitalwatchdog_global": {
        "release_notes": "http://digital-watchdog.com/techbulletindetail/DW-Spectrum-Software-Update-v-2-5-0-11500-/",
        "updates_prefix": "http://updates.networkoptix.com/digitalwatchdog_global",
        "releases": {
            "2.3": "2.3.2.9503",
            "2.2": "2.2.1.3217",
            "2.4": "2.4.1.10278",
            "3.0": "3.0.0.15297",
            "3.1": "3.1.0.16975",
            "2.5": "2.5.0.11500",
            "2.6": "2.6.0.13109"
        },
        "download_version": "3.0.0.15297",
        "current_release": "3.0",
        "description": "",
        "release_date": "1503071973772",
        "release_delivery": 15
    },
    "vmsdemoblue_zh_CN": {
        "release_notes": "http://updates.hdwitness.com/release_notes.html",
        "updates_prefix": "http://updates.networkoptix.com/vmsdemoblue_zh_CN",
        "releases": {
            "2.3": "2.3.0.8344",
            "2.4": "2.4.0.0",
            "2.2": "2.2.1.3217"
        }
    },
    "av": {
        "release_notes": "http://updates.hdwitness.com/release_notes.html",
        "updates_prefix": "http://updates.networkoptix.com/av",
        "releases": {
            "2.3": "2.3.0.6916",
            "2.4": "2.4.0.0",
            "2.2": "2.2.1.3217"
        }
    },
    "relong": {
        "release_notes": "http://updates.hdwitness.com/release_notes.html",
        "updates_prefix": "http://updates.networkoptix.com/relong",
        "releases": {
            "2.3": "2.3.0.8177",
            "2.4": "2.4.0.0",
            "2.2": "2.2.1.3217"
        }
    },
    "default": {
        "release_notes": "http://www.networkoptix.com/all-nx-witness-release-notes",
        "updates_prefix": "http://updates.networkoptix.com/default",
        "releases": {
            "2.2": "2.2.1.3217",
            "2.3": "2.3.2.9167",
            "2.4": "2.4.1.10278",
            "17.2": "17.2.3.15624",
            "3.0": "3.0.0.15297",
            "2.5": "2.5.0.11500",
            "2.6": "2.6.0.13109",
            "3.1": "3.1.0.16975"
        },
        "download_version": "3.1.0.16975",
        "current_release": "3.1",
        "description": "",
        "release_date": "1510776791034",
        "release_delivery": 30
    },
    "tricom": {
        "release_notes": "http://www.networkoptix.com/all-nx-witness-release-notes",
        "updates_prefix": "http://updates.networkoptix.com/tricom",
        "releases": {
            "2.3": "2.3.2.9167",
            "2.4": "2.4.1.10686",
            "2.5": "2.5.0.11376",
            "2.2": "2.2.1.3217",
            "17.2": "17.2.3.15624",
            "3.0": "3.0.0.14532",
            "2.6": "2.6.0.13109"
        },
        "download_version": "2.6.0.13109",
        "current_release": "2.6",
        "description": "",
        "release_date": "1496701410312",
        "release_delivery": 30
    },
    "ipera": {
        "release_notes": "http://updates.flyviewvms.ru/release_notes.html",
        "updates_prefix": "http://updates.networkoptix.com/ipera",
        "releases": {
            "2.3": "2.3.2.9167",
            "2.2": "2.2.1.3217",
            "2.4": "2.4.1.10278",
            "2.5": "2.5.0.11500",
            "17.2": "17.2.3.15624",
            "3.0": "3.0.0.15297",
            "2.6": "2.6.0.13109",
            "3.1": "3.1.0.16975"
        },
        "download_version": "3.1.0.16975",
        "current_release": "3.1",
        "description": "",
        "release_date": "1510776791649",
        "release_delivery": 30
    },
    "vmsdemoblue": {
        "release_notes": "http://www.networkoptix.com/all-nx-witness-release-notes",
        "updates_prefix": "http://updates.networkoptix.com/vmsdemoblue",
        "releases": {
            "2.2": "2.2.1.3217",
            "2.3": "2.3.2.9167",
            "2.4": "2.4.1.10278",
            "3.0": "3.0.0.15297",
            "2.5": "2.5.0.11500",
            "2.6": "2.6.0.13109",
            "3.1": "3.1.0.16975"
        },
        "download_version": "3.1.0.16975",
        "current_release": "3.1",
        "description": "",
        "release_date": "1510776791187",
        "release_delivery": 30
    },
    "nutech": {
        "release_notes": "http://digital-watchdog.com/techbulletindetail/DW-Spectrum-Software-Update-v-2-5-0-11500-/",
        "updates_prefix": "http://updates.networkoptix.com/nutech",
        "releases": {
            "2.3": "2.3.2.9167",
            "2.2": "2.2.1.3217",
            "2.4": "2.4.1.10278",
            "3.0": "3.0.0.15297",
            "3.1": "3.1.0.16975",
            "2.5": "2.5.0.11500",
            "2.6": "2.6.0.13109"
        },
        "download_version": "3.0.0.15297",
        "current_release": "3.0",
        "description": "",
        "release_date": "1503071973917",
        "release_delivery": 15
    },
    "cox": {
        "release_notes": "???",
        "updates_prefix": "http://updates.networkoptix.com/cox",
        "releases": {
            "3.0": "3.0.0.14971",
            "17.2": "17.2.3.15624",
            "3.1": "3.1.0.16975",
            "2.5": "2.5.0.12221"
        },
        "download_version": "2.5.0.12221",
        "current_release": "2.5",
        "description": ""
    },
    "vista": {
        "release_notes": "http://updates.vista-cctv.com/release_notes_vista.html",
        "updates_prefix": "http://updates.vista-cctv.com/vista",
        "releases": {
            "2.2": "2.2.1.3217",
            "2.3": "2.3.2.9167",
            "2.4": "2.4.1.10278",
            "17.2": "17.2.3.15624",
            "3.1": "3.1.0.16975",
            "3.0": "3.0.0.15297",
            "2.5": "2.5.0.11500",
            "2.6": "2.6.0.13109"
        },
        "download_version": "3.0.0.15297",
        "current_release": "3.0",
        "description": "",
        "release_date": "1499390692085",
        "release_delivery": 30
    },
    "digitalwatchdog": {
        "release_notes": "http://digital-watchdog.com/techbulletindetail/DW-Spectrum-IPVMS-v2-6-0-13109-Update-2016-12-06/",
        "updates_prefix": "http://updates.networkoptix.com/digitalwatchdog",
        "releases": {
            "2.2": "2.2.1.3217",
            "2.3": "2.3.2.9167",
            "2.4": "2.4.1.10278",
            "3.0": "3.0.0.15297",
            "17.2": "17.2.3.15624",
            "3.1": "3.1.0.16975",
            "2.5": "2.5.0.11500",
            "2.6": "2.6.0.13109"
        },
        "download_version": "2.6.0.13109",
        "current_release": "2.6",
        "description": "",
        "release_date": "1481144126523",
        "release_delivery": 30
    },
    "default_cn": {
        "release_notes": "http://www.networkoptix.com/all-nx-witness-release-notes",
        "updates_prefix": "http://updates.networkoptix.com/default_cn",
        "releases": {
            "2.2": "2.2.1.3217",
            "2.3": "2.3.2.9167",
            "2.4": "2.4.1.10278",
            "3.0": "3.0.0.15297",
            "2.5": "2.5.0.11500",
            "2.6": "2.6.0.13109",
            "3.1": "3.1.0.16975"
        },
        "download_version": "3.1.0.16975",
        "current_release": "3.1",
        "description": "",
        "release_date": "1510776791114",
        "release_delivery": 30
    },
    "default_zh_CN": {
        "release_notes": "http://www.networkoptix.com/all-nx-witness-release-notes",
        "updates_prefix": "http://updates.networkoptix.com/default_zh_CN",
        "releases": {
            "2.2": "2.2.1.3217",
            "2.3": "2.3.2.9167",
            "2.4": "2.4.1.10278",
            "3.0": "3.0.0.15297",
            "2.5": "2.5.0.11500",
            "2.6": "2.6.0.13109",
            "3.1": "3.1.0.16975"
        },
        "download_version": "3.1.0.16975",
        "current_release": "3.1",
        "description": "",
        "release_date": "1510776791513",
        "release_delivery": 30
    },
    "relong_en": {
        "release_notes": "???",
        "updates_prefix": "http://updates.networkoptix.com/relong_en",
        "releases": {}
    },
    "vmsdemoblue_cn": {
        "release_notes": "???",
        "updates_prefix": "http://updates.networkoptix.com/vmsdemoblue_cn",
        "releases": {}
    },
    "nnodal_zh_CN": {
        "release_notes": "http://www.networkoptix.com/all-nx-witness-release-notes",
        "updates_prefix": "http://updates.networkoptix.com/nnodal_zh_CN",
        "releases": {
            "2.3": "2.3.2.9167",
            "2.4": "2.4.1.10278",
            "2.5": "2.5.0.11500",
            "2.6": "2.6.0.13109"
        }
    },
    "ionetworks": {
        "release_notes": "???",
        "updates_prefix": "http://updates.networkoptix.com/ionetworks",
        "releases": {
            "2.6": "2.6.0.13254",
            "3.0": "3.0.0.15297",
            "17.2": "17.2.3.15624",
            "3.1": "3.1.0.16975"
        },
        "download_version": "3.1.0.16975",
        "current_release": "3.1",
        "description": "",
        "release_date": "1510776791272",
        "release_delivery": 30
    },
    "pcms": {
        "release_notes": "???",
        "updates_prefix": "http://updates.networkoptix.com/pcms",
        "releases": {
            "2.3": "2.3.2.9167",
            "2.4": "2.4.0.9813"
        }
    },
    "pivothead": {
        "release_notes": "???",
        "updates_prefix": "http://updates.networkoptix.com/pivothead",
        "releases": {}
    },
    "ust": {
        "release_notes": "???",
        "updates_prefix": "http://updates.networkoptix.com/ust",
        "releases": {
            "3.0": "3.0.0.15297",
            "17.2": "17.2.3.15624",
            "2.6": "2.6.0.13109",
            "3.1": "3.1.0.16975"
        },
        "download_version": "3.1.0.16975",
        "current_release": "3.1",
        "description": "",
        "release_date": "1510776791411",
        "release_delivery": 30
    },
    "senturian": {
        "release_notes": "http://updates.hdwitness.com/release_notes.html",
        "updates_prefix": "http://updates.networkoptix.com/senturian",
        "releases": {
            "3.0": "3.0.0.15297",
            "17.2": "17.2.3.15624",
            "2.6": "2.6.0.14313",
            "3.1": "3.1.0.16975"
        },
        "download_version": "3.1.0.16975",
        "current_release": "3.1",
        "description": "",
        "release_date": "1510776791344",
        "release_delivery": 30
    },
    "systemk": {
        "release_notes": "http://updates.hdwitness.com/release_notes.html",
        "updates_prefix": "http://updates.networkoptix.com/systemk",
        "releases": {
            "17.2": "17.2.3.15624",
            "3.0": "3.0.0.15297",
            "3.1": "3.1.0.16975"
        },
        "download_version": "3.1.0.16975",
        "current_release": "3.1",
        "description": "",
        "release_date": "1510776791967",
        "release_delivery": 30
    },
    "ras": {
        "release_notes": "http://updates.hdwitness.com/release_notes.html",
        "updates_prefix": "http://updates.networkoptix.com/ras",
        "releases": {
            "3.0": "3.0.0.15359",
            "17.2": "17.2.3.15624",
            "3.1": "3.1.0.16975"
        },
        "download_version": "3.1.0.16975",
        "current_release": "3.1",
        "description": "",
        "release_date": "1510776791798",
        "release_delivery": 30
    },
    "hanwha": {
        "release_notes": "https://www.hanwhasecurity.com/wave-release-notes",
        "updates_prefix": "http://updates.wavevms.com/hanwha",
        "releases": {
            "3.0": "3.0.0.14971",
            "3.1": "3.1.0.15816"
        },
        "download_version": "3.1.0.15816",
        "current_release": "3.1",
        "description": "",
        "release_date": "1508351329240",
        "release_delivery": 30
    }
}
)JSON";
    return result;
}

const std::vector<UpdateTestData>& updateTestDataList()
{
    static const std::vector<UpdateTestData> result = {
{
    "default",
    "16975",
    R"JSON(
    {
        "version": "3.1.0.16975",
        "cloudHost": "nxvms.com",
        "packages": {
            "linux": {
                "arm_rpi": {
                    "file": "nxwitness-server_update-3.1.0.16975-rpi.zip",
                    "size": 80605107,
                    "md5": "211a118de0de617799383b9202139cd7"
                },
                "arm_bpi": {
                    "file": "nxwitness-server_update-3.1.0.16975-bpi.zip",
                    "size": 147835827,
                    "md5": "0ab4bcce79193d5dc3d84f77a7e7b640"
                },
                "arm_bananapi": {
                    "file": "nxwitness-server_update-3.1.0.16975-bananapi.zip",
                    "size": 64702397,
                    "md5": "977d21111bb4b6f761da878b670cf479"
                },
                "x64_ubuntu": {
                    "file": "nxwitness-server_update-3.1.0.16975-linux64.zip",
                    "size": 83795048,
                    "md5": "586edd0681dc341aebdfbc5d456669e1"
                },
                "x86_ubuntu": {
                    "file": "nxwitness-server_update-3.1.0.16975-linux86.zip",
                    "size": 101335323,
                    "md5": "1a4b6166c3af321455dbe05e63b6826f"
                }
            },
            "windows": {
                "x64_winxp": {
                    "file": "nxwitness-server_update-3.1.0.16975-win64.zip",
                    "size": 74358355,
                    "md5": "58e6395c2b31ba1215b631855ced3827"
                },
                "x86_winxp": {
                    "file": "nxwitness-server_update-3.1.0.16975-win86.zip",
                    "size": 65677361,
                    "md5": "3b809d81dd60e3377fe5962c2de48e8c"
                }
            }
        },
        "clientPackages": {
            "linux": {
                "x86_ubuntu": {
                    "file": "nxwitness-client_update-3.1.0.16975-linux86.zip",
                    "size": 101992185,
                    "md5": "3e74e6c296abbfb0a9f3ff9475a915d3"
                },
                "x64_ubuntu": {
                    "file": "nxwitness-client_update-3.1.0.16975-linux64.zip",
                    "size": 103335409,
                    "md5": "5181a6ce5c6991c8a632c0d443f36f2b"
                }
            },
            "macosx": {
                "x64_macosx": {
                    "file": "nxwitness-client_update-3.1.0.16975-mac.zip",
                    "size": 77903157,
                    "md5": "88a9225b97d56282325a14ec39af7459"
                }
            },
            "windows": {
                "x64_winxp": {
                    "file": "nxwitness-client_update-3.1.0.16975-win64.zip",
                    "size": 86289814,
                    "md5": "071e18fdaddf9faeab2fe34839dd2d19"
                },
                "x86_winxp": {
                    "file": "nxwitness-client_update-3.1.0.16975-win86.zip",
                    "size": 74931495,
                    "md5": "409f942585e2d5df59b7f891f1f94e5c"
                }
            }
        },
        "description": ""
    }
    )JSON"
},
{
    "tricom",
    "14532",
    R"JSON(
    {
        "packages": {
            "windows": {
                "x64_winxp": {
                    "size": 74254007,
                    "md5": "4c94b0d4c1af28aece7cd4d2fb5d246e",
                    "file": "tricom-server_update-3.0.0.14532-win64-beta-demo.zip"
                },
                "x86_winxp": {
                    "size": 65251780,
                    "md5": "26967fd981284a7a0f3744de6c7f1fe8",
                    "file": "tricom-server_update-3.0.0.14532-win86-beta-demo.zip"
                }
            },
            "linux": {
                "arm_bpi": {
                    "size": 116491200,
                    "md5": "5fe1e4e13834ee88028f105e8c33245d",
                    "file": "tricom-server_update-3.0.0.14532-bpi-beta-demo.zip"
                },
                "x86_ubuntu": {
                    "size": 76084528,
                    "md5": "5a882a0be611871e0d6af44c4d7ad015",
                    "file": "tricom-server_update-3.0.0.14532-linux86-beta-demo.zip"
                },
                "arm_bananapi": {
                    "size": 56097236,
                    "md5": "2fb5c9de47d5e9cef73e13702dec4479",
                    "file": "tricom-server_update-3.0.0.14532-bananapi-beta-demo.zip"
                },
                "x64_ubuntu": {
                    "size": 59897502,
                    "md5": "c6db3f80b51370dc7b9c37a10bae06c7",
                    "file": "tricom-server_update-3.0.0.14532-linux64-beta-demo.zip"
                },
                "arm_rpi": {
                    "size": 70972113,
                    "md5": "e7207382eaccb1970219cd80aed3dd6c",
                    "file": "tricom-server_update-3.0.0.14532-rpi-beta-demo.zip"
                }
            }
        },
        "version": "3.0.0.14532",
        "clientPackages": {
            "windows": {
                "x64_winxp": {
                    "size": 81117391,
                    "md5": "df1ea9c2be817f0eb922042cd3f4784e",
                    "file": "tricom-client_update-3.0.0.14532-win64-beta-demo.zip"
                },
                "x86_winxp": {
                    "size": 69441539,
                    "md5": "3d2c6fd416897eed543a8dacbe465f8f",
                    "file": "tricom-client_update-3.0.0.14532-win86-beta-demo.zip"
                }
            },
            "macosx": {
                "x64_macosx": {
                    "size": 67998174,
                    "md5": "301259d83a85b9d9929c2875da204ebb",
                    "file": "tricom-client_update-3.0.0.14532-mac-beta-demo.zip"
                }
            },
            "linux": {
                "x86_ubuntu": {
                    "size": 88679924,
                    "md5": "eafe8cf6ca85b8fe3c7619adeb51b4e9",
                    "file": "tricom-client_update-3.0.0.14532-linux86-beta-demo.zip"
                },
                "x64_ubuntu": {
                    "size": 89920467,
                    "md5": "8d55fde8c0e278d7edd7d4bd82257a9b",
                    "file": "tricom-client_update-3.0.0.14532-linux64-beta-demo.zip"
                }
            }
        },
        "cloudHost": "tricom.cloud-demo.hdw.mx"
    }
    )JSON"
},
{
    "vista",
    "16975",
    R"JSON(
    {
        "version": "3.1.0.16975",
        "cloudHost": "qcloud.vista-cctv.com",
        "packages": {
            "linux": {
                "arm_bpi": {
                    "file": "qulu-server_update-3.1.0.16975-bpi.zip",
                    "size": 148280781,
                    "md5": "ed671c1926374065d32a1611c1bcd610"
                },
                "arm_rpi": {
                    "file": "qulu-server_update-3.1.0.16975-rpi.zip",
                    "size": 80961694,
                    "md5": "24f894201cc713288505cf5922422e30"
                },
                "arm_bananapi": {
                    "file": "qulu-server_update-3.1.0.16975-bananapi.zip",
                    "size": 65046981,
                    "md5": "1a6a27cf559a35ef8381fc8dedb52c70"
                },
                "x86_ubuntu": {
                    "file": "qulu-server_update-3.1.0.16975-linux86.zip",
                    "size": 100352644,
                    "md5": "206504491d7d7a5462de0be6e7b021cb"
                },
                "x64_ubuntu": {
                    "file": "qulu-server_update-3.1.0.16975-linux64.zip",
                    "size": 82659980,
                    "md5": "69d7d64b99e6de5edac30f53303c9069"
                }
            },
            "windows": {
                "x64_winxp": {
                    "file": "qulu-server_update-3.1.0.16975-win64.zip",
                    "size": 74558108,
                    "md5": "458cdfea469d191d0e9516907be27627"
                },
                "x86_winxp": {
                    "file": "qulu-server_update-3.1.0.16975-win86.zip",
                    "size": 65876062,
                    "md5": "5eb51b416f193a9736982922dffab750"
                }
            }
        },
        "clientPackages": {
            "macosx": {
                "x64_macosx": {
                    "file": "qulu-client_update-3.1.0.16975-mac.zip",
                    "size": 75583681,
                    "md5": "c64fd2f5413f5233d8f8d0fe13004410"
                }
            },
            "windows": {
                "x86_winxp": {
                    "file": "qulu-client_update-3.1.0.16975-win86.zip",
                    "size": 72599226,
                    "md5": "3849af95d170e4b3e32ecf563410f9ad"
                },
                "x64_winxp": {
                    "file": "qulu-client_update-3.1.0.16975-win64.zip",
                    "size": 83955425,
                    "md5": "2c2489c1da16df190784fdb061553beb"
                }
            },
            "linux": {
                "x64_ubuntu": {
                    "file": "qulu-client_update-3.1.0.16975-linux64.zip",
                    "size": 101055125,
                    "md5": "3b7e582a985ab5a914f6f95f4a7c5c32"
                },
                "x86_ubuntu": {
                    "file": "qulu-client_update-3.1.0.16975-linux86.zip",
                    "size": 99726070,
                    "md5": "fcd5546075481e08a1cf7a2ffab7bbf4"
                }
            }
        },
        "description": ""
    }
    )JSON"
}
};
    return result;
};

} // namespace test_support
} // namespace data_parser
} // namespace detail
} // namespace info
} // namespace update
} // namespace nx

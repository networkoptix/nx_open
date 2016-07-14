using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace NetworkOptix {
    class Util {
        public static Version shortenVersion(string version) {
            Version fullVersion = new Version(version);
            return new Version(fullVersion.Major, fullVersion.Minor);
        }
    }
}

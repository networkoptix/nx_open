using System.Collections.Generic;
using System.Windows.Forms;
using Paxton.Net2.OemDvrInterfaces;
using Paxton.NetworkOptixControl;

namespace NetworkOptix.NxWitness.OemDvrMiniDriver {

    public class NetworkOptixMiniDriver : IOemDvrMiniDriver
    {
        private static NetworkOptixDriver _driver = new NetworkOptixDriver();

        public string GetSupplierIdentity()
        {
            return _driver.GetSupplierIdentity();
        }

        public OemDvrStatus VerifyDvrCredentials(OemDvrConnection connectionInfo)
        {
            return _driver.VerifyDvrCredentials(connectionInfo);
        }

        public OemDvrStatus GetListOfCameras(OemDvrConnection connectionInfo, List<OemDvrCamera> cameras)
        {
            return _driver.GetListOfCameras(connectionInfo, cameras);
        }

        public OemDvrStatus PlayFootage(OemDvrConnection cnInfo, OemDvrFootageRequest pbInfo, Panel pbSurface)
        {
            return _driver.PlayFootage(cnInfo, pbInfo, pbSurface);
        }

        public OemDvrStatus ControlFootage(OemDvrPlaybackFunction pbFunction, uint speed)
        {
            return _driver.ControlFootage(pbFunction, speed);
        }
    }


}

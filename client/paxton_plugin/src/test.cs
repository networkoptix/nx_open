using System;
using System.Collections.Generic;
using Paxton.Net2.OemDvrInterfaces;

namespace nx
{
    public class Test
    {
        static void Main()
        {
            var connectionInfo = new OemDvrConnection("localhost", 0, "admin", "qweasd123");
            PaxtonClient.testConnection(connectionInfo);

            var cameras = new List<OemDvrCamera>();
            foreach (var camera in PaxtonClient.requestCameras(connectionInfo))
            {
                Console.WriteLine(camera.CameraId + ": " + camera.CameraName);
                cameras.Add(camera);
            }

            PaxtonClient.playback(connectionInfo, new OemDvrFootageRequest(
                DateTime.Now - TimeSpan.FromMinutes(5),
                OemDvrPlaybackFunction.Start,
                100,
                cameras.ToArray()));
            Console.ReadKey();
        }
    }
}

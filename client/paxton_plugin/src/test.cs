using System;
using System.Collections.Generic;

namespace nx
{
    public class Test
    {
        static void Main()
        {
            var impl = new PaxtonClient("localhost", 0, "admin", "qweasd123");
            impl.testConnection();

            var cameras = new List<string>();
            foreach (var camera in impl.requestCameras())
            {
                Console.WriteLine(camera.id + ": " + camera.name);
                cameras.Add(camera.id);
            }
            impl.playback(cameras, DateTime.Now - TimeSpan.FromMinutes(5), 100);
            Console.ReadKey();
        }
    }
}

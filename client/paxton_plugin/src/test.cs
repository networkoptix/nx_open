using System;

namespace nx
{
    public class Test
    {
        static void Main()
        {
            var impl = new PaxtonClient("localhost", 0, "admin", "qweasd123");
            impl.testConnection();
            foreach (var camera in impl.requestCameras())
                Console.WriteLine(camera.id + ": " + camera.name);

            Console.ReadKey();
        }
    }
}

namespace nx
{
    public class Test
    {
        static void Main()
        {
            var impl = new PaxtonClient("localhost", 0, "admin", "qweasd123");
            impl.testConnection();
        }
    }
}

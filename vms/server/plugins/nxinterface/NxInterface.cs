using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace NxInterface
{
    public class ConnectionProcessedEvent {
        public string p_message;
        public int p_status;

        public ConnectionProcessedEvent(int p_status, string p_message) {
            this.p_status = p_status;
            this.p_message = p_message;
        }
    }

    public delegate void ConnectionProcessedEventHandler(object sender, ConnectionProcessedEvent e);

    public interface IPlugin {
        void play();
        void pause();
        double speed();
        void setSpeed(double speed);
        void reconnect(string url);
        void addResourcesToLayout(string resourceIds, string timestamp);

        event ConnectionProcessedEventHandler connectProcessed;
    }
}

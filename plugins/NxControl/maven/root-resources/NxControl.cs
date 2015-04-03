using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using NxInterface;
using AxInterop.${ax.className};

namespace NxControl
{
    public class NxControl : AxAx${ax.className}, IPlugin
    {
        public NxControl() {
            base.connectionProcessed += ax${ax.className}1_connectedProcessed;
        }

        public event ConnectionProcessedEventHandler connectProcessed;
        void ax${ax.className}1_connectedProcessed(object sender, IAx${ax.className}Events_connectionProcessedEvent e) {
            connectProcessed(sender, new ConnectionProcessedEvent(e.p_status, e.p_message));
        }
    }
}

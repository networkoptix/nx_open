using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using NxInterface;
using AxInterop.${paxton.className}_${nxec.ec2ProtoVersion};

namespace NxControl
{
    public class NxControl : AxAx${paxton.className}, IPlugin
    {
        public NxControl() {
            base.connectionProcessed += ax${paxton.className}1_connectedProcessed;
        }

        public event ConnectionProcessedEventHandler connectProcessed;
        void ax${paxton.className}1_connectedProcessed(object sender, IAx${paxton.className}Events_connectionProcessedEvent e) {
            connectProcessed(sender, new ConnectionProcessedEvent(e.p_status, e.p_message));
        }
    }
}

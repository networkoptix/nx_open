.import com.networkoptix.qml 1.0 as Nx

function connectionErrorText(status, info)
{
    if (status == Nx.QnConnectionManager.UnauthorizedConnectionResult)
        return qsTr("Invalid login or password")
    else if (status == Nx.QnConnectionManager.TemporaryUnauthorizedConnectionResult)
        return qsTr("LDAP Server connection timed out")
    else if (status == Nx.QnConnectionManager.NetworkErrorConnectionResult)
        return qsTr("Server or network is not available")
    else if (status == Nx.QnConnectionManager.IncompatibleInternalConnectionResult)
        return qsTr("Incompatible server")
    else if (status == Nx.QnConnectionManager.IncompatibleVersionConnectionResult)
        return qsTr("Incompatible server version %1").arg(info)
    return ""
}

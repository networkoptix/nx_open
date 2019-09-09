.import com.networkoptix.qml 1.0 as Nx

function lockoutConnectionErrorText()
{
    return qsTr("Too many attempts. Try again in a minute.")
}

function connectionErrorText(status, info)
{
    if (status == Nx.QnConnectionManager.UnauthorizedConnectionResult)
        return qsTr("Invalid login or password")
    else if (status == Nx.QnConnectionManager.LdapTemporaryUnauthorizedConnectionResult)
        return qsTr("LDAP Server connection timed out")
    else if (status == Nx.QnConnectionManager.NetworkErrorConnectionResult)
        return qsTr("Server or network is not available")
    else if (status == Nx.QnConnectionManager.IncompatibleInternalConnectionResult)
        return qsTr("Incompatible server")
    else if (status == Nx.QnConnectionManager.IncompatibleVersionConnectionResult)
        return qsTr("Incompatible server version %1").arg(info)
    else if (status == Nx.QnConnectionManager.FactoryServerConnectionResult)
        return qsTr("Connect to this server from web browser or through desktop client to set it up")
    else if (status == Nx.QnConnectionManager.UserTemporaryLockedOut)
        return lockoutConnectionErrorText()
    return ""
}

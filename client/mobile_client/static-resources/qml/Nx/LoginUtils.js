.import com.networkoptix.qml 1.0 as Nx

function makeUrl(address, login, password, cloud)
{
    var result =
            (cloud ? "cloud" : "http") +
            "://" +
            encodeURIComponent(login) +
            ":" +
            encodeURIComponent(password) +
            "@" +
            address
    return result
}

function connectionErrorText(status, info)
{
    if (status == Nx.QnConnectionManager.Unauthorized)
        return qsTr("Invalid login or password")
    else if (status == Nx.QnConnectionManager.TemporaryUnauthorized)
        return qsTr("LDAP Server connection timed out")
    else if (status == Nx.QnConnectionManager.NetworkError)
        return qsTr("Server or network is not available")
    else if (status == Nx.QnConnectionManager.IncompatibleInternal)
        return qsTr("Incompatible server")
    else if (status == Nx.QnConnectionManager.IncompatibleVersion)
        return qsTr("Incompatible server version %1").arg(info)
    return ""
}

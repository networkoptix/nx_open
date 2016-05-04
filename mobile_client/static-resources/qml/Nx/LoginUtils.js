.import com.networkoptix.qml 1.0 as Nx

function makeUrl(host, port, login, password)
{
    var result = "http://" + login + ":" + password + "@" + host
    if (port)
        result += ":" + port
    return result
}

function connectionErrorText(status, info)
{
    if (status == Nx.QnConnectionManager.Unauthorized)
        return qsTr("Incorrect login or password")
    else if (status == Nx.QnConnectionManager.NetworkError)
        return qsTr("Server or network is not available")
    else if (status == Nx.QnConnectionManager.InvalidServer)
        return qsTr("Incompatible server")
    else if (status == Nx.QnConnectionManager.InvalidVersion)
        return qsTr("Incompatible server version %1").arg(info)
    return ""
}

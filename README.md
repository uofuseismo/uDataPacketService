# About

This is a highly-available utility that reads packets from the [uDataPacketImportProxy](https://github.com/uofuseismo/uDataPacketImportProxy.git) and serves these packets to interested clients in the UUSS K8s cluster.  Clients can subscribe to a subset of streams and screen data so as to minimize duplicate packets, expired data, and future data.

# Proto Files

To obtain the proto files prior to compiling this software do the following:

    git subtree add --prefix uDataPacketImportAPI https://github.com/uofuseismo/uDataPacketImportAPI.git main --squash
    git subtree add --prefix uDataPacketServiceAPI https://github.com/uofuseismo/uDataPacketServiceAPI.git main --squash

To update the proto files use

    git subtree pull --prefix uDataPacketImportAPI https://github.com/uofuseismo/uDataPacketImportAPI.git main --squash
    git subtree pull --prefix uDataPacketServiceAPI https://github.com/uofuseismo/uDataPacketServiceAPI.git main --squash



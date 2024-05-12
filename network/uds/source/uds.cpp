#include <iostream>
#include <cstring>
#include <3ds.h>

void print_constatus()
{
    Result ret=0;
    u32 pos;
    udsConnectionStatus constatus;

    //By checking the output of udsGetConnectionStatus you can check for nodes (including the current one) which just (dis)connected, etc.
    ret = udsGetConnectionStatus(&constatus);
    if(R_FAILED(ret))
    {
        std::cout << "udsGetConnectionStatus() returned 0x" << std::hex << ret << std::endl;
    }
    else
    {
        std::cout << "constatus:" << std::endl;
        std::cout << "status=0x" << std::hex << constatus.status << std::endl;
        std::cout << "1=0x" << std::hex << constatus.unk_x4 << std::endl;
        std::cout << "cur_NetworkNodeID=0x" << std::hex << constatus.cur_NetworkNodeID << std::endl;
        std::cout << "unk_xa=0x" << std::hex << constatus.unk_xa << std::endl;
        for(pos=0; pos<(0x20>>2); pos++) std::cout << pos+3 << "=0x" << std::hex << constatus.unk_xc[pos] << " ";
        std::cout << std::endl;
        std::cout << "total_nodes=0x" << std::hex << constatus.total_nodes << std::endl;
        std::cout << "max_nodes=0x" << std::hex << constatus.max_nodes << std::endl;
        std::cout << "node_bitmask=0x" << std::hex << constatus.total_nodes << std::endl;
    }
}

void uds_test()
{
    Result ret=0;
    u32 con_type=0;

    u32 *tmpbuf;
    size_t tmpbuf_size;

    u8 data_channel = 1;
    udsNetworkStruct networkstruct;
    udsBindContext bindctx;
    udsNetworkScanInfo *networks = NULL;
    udsNetworkScanInfo *network = NULL;
    size_t total_networks = 0;

    u32 recv_buffer_size = UDS_DEFAULT_RECVBUFSIZE;
    u32 wlancommID = 0x48425710;//Unique ID, change this to your own.
    char *passphrase = "udsdemo passphrase c186093cd2652741";//Change this passphrase to your own. The input you use for the passphrase doesn't matter since it's a raw buffer.

    udsConnectionType conntype = UDSCONTYPE_Client;

    u32 transfer_data, prev_transfer_data = 0;
    size_t actual_size;
    u16 src_NetworkNodeID;
    u32 tmp=0;
    u32 pos;

    udsNodeInfo tmpnode;

    u8 appdata[0x14] = {0x69, 0x8a, 0x05, 0x5c};
    u8 out_appdata[0x14];

    char tmpstr[256];

    strncpy((char*)&appdata[4], "Test appdata.", sizeof(appdata)-4);

    std::cout << "Successfully initialized." << std::endl;

    tmpbuf_size = 0x4000;
    tmpbuf = static_cast<u32*>(malloc(tmpbuf_size));
    if(tmpbuf==NULL)
    {
        std::cout << "Failed to allocate tmpbuf for beacon data." << std::endl;
        return;
    }

    //With normal client-side handling you'd keep running network-scanning until the user chooses to stops scanning or selects a network to connect to. This example just scans a maximum of 10 times until at least one network is found.
    for(pos=0; pos<10; pos++)
    {
        total_networks = 0;
        memset(tmpbuf, 0, sizeof(tmpbuf_size));
        ret = udsScanBeacons(tmpbuf, tmpbuf_size, &networks, &total_networks, wlancommID, 0, NULL, false);
        std::cout << "udsScanBeacons() returned 0x" << std::hex << ret << "." << std::endl;
        std::cout << "total_networks=" << total_networks << "." << std::endl;

        if(total_networks) break;
    }

    free(tmpbuf);
    tmpbuf = NULL;

    if(total_networks)
    {
        //At this point you'd let the user select which network to connect to and optionally display the first node's username(the host), along with the parsed appdata if you want. For this example this just uses the first detected network and then displays the username of each node.
        //If appdata isn't enough, you can do what DLP does loading the icon data etc: connect to the network as a spectator temporarily for receiving broadcasted data frames.

        network = &networks[0];

        std::cout << "network: total nodes = " << network->network.total_nodes << "." << std::endl;

        for(pos=0; pos<UDS_MAXNODES; pos++)
        {
            if(!udsCheckNodeInfoInitialized(&network->nodes[pos])) continue;

            memset(tmpstr, 0, sizeof(tmpstr));

            ret = udsGetNodeInfoUsername(&network->nodes[pos], tmpstr);
            if(R_FAILED(ret))
            {
                std::cout << "udsGetNodeInfoUsername() returned 0x" << std::hex << ret << "." << std::endl;
                free(networks);
                return;
            }

            std::cout << "node" << pos << " username: " << tmpstr << std::endl;
        }

        //You can load appdata from the scanned beacon data here if you want.
        actual_size = 0;
        ret = udsGetNetworkStructApplicationData(&network->network, out_appdata, sizeof(out_appdata), &actual_size);
        if(R_FAILED(ret) || actual_size!=sizeof(out_appdata))
        {
            std::cout << "udsGetNetworkStructApplicationData() returned 0x" << std::hex << ret << ". actual_size = 0x" << actual_size << "." << std::endl;
            free(networks);
            return;
        }

        memset(tmpstr, 0, sizeof(tmpstr));
        if(memcmp(out_appdata, appdata, 4)!=0)
        {
            std::cout << "The first 4-bytes of appdata is invalid." << std::endl;
            free(networks);
            return;
        }

        strncpy(tmpstr, (char*)&out_appdata[4], sizeof(out_appdata)-5);
        tmpstr[sizeof(out_appdata)-6]='\0';

        std::cout << "String from network appdata: " << (char*)&out_appdata[4] << std::endl;

        hidScanInput();//Normally you would only connect as a regular client.
        if(hidKeysHeld() & KEY_L)
        {
            conntype = UDSCONTYPE_Spectator;
            std::cout << "Connecting to the network as a spectator..." << std::endl;
        }
        else
        {
            std::cout << "Connecting to the network as a client..." << std::endl;
        }

        for(pos=0; pos<10; pos++)
        {
            ret = udsConnectNetwork(&network->network, passphrase, strlen(passphrase)+1, &bindctx, UDS_BROADCAST_NETWORKNODEID, conntype, data_channel, recv_buffer_size);
            if(R_FAILED(ret))
            {
                std::cout << "udsConnectNetwork() returned 0x" << std::hex << ret << "." << std::endl;
            }
            else
            {
                break;
            }
        }

        free(networks);

        if(pos==10) return;

        std::cout << "Connected." << std::endl;

        tmp = 0;
        ret = udsGetChannel((u8*)&tmp);//Normally you don't need to use this.
        std::cout << "udsGetChannel() returned 0x" << std::hex << ret << ". channel = " << tmp << "." << std::endl;
        if(R_FAILED(ret))
        {
            return;
        }

        //You can load the appdata with this once connected to the network, if you want.
        memset(out_appdata, 0, sizeof(out_appdata));
        actual_size = 0;
        ret = udsGetApplicationData(out_appdata, sizeof(out_appdata), &actual_size);
        if(R_FAILED(ret) || actual_size!=sizeof(out_appdata))
        {
            std::cout << "udsGetApplicationData() returned 0x" << std::hex << ret << ". actual_size = 0x" << actual_size << "." << std::endl;
            udsDisconnectNetwork();
            udsUnbind(&bindctx);
            return;
        }

        memset(tmpstr, 0, sizeof(tmpstr));
        if(memcmp(out_appdata, appdata, 4)!=0)
        {
            std::cout << "The first 4-bytes of appdata is invalid." << std::endl;
            udsDisconnectNetwork();
            udsUnbind(&bindctx);
            return;
        }

        strncpy(tmpstr, (char*)&out_appdata[4], sizeof(out_appdata)-5);
        tmpstr[sizeof(out_appdata)-6]='\0';

        std::cout << "String from appdata: " << (char*)&out_appdata[4] << std::endl;

        con_type = 1;
    }
    else
    {
        udsGenerateDefaultNetworkStruct(&networkstruct, wlancommID, 0, UDS_MAXNODES);

        std::cout << "Creating the network..." << std::endl;
        ret = udsCreateNetwork(&networkstruct, passphrase, strlen(passphrase)+1, &bindctx, data_channel, recv_buffer_size);
        if(R_FAILED(ret))
        {
            std::cout << "udsCreateNetwork() returned 0x" << std::hex << ret << "." << std::endl;
            return;
        }

        ret = udsSetApplicationData(appdata, sizeof(appdata));//If you want to use appdata, you can set the appdata whenever you want after creating the network. If you need more space for appdata, you can set different chunks of appdata over time.
        if(R_FAILED(ret))
        {
            std::cout << "udsSetApplicationData() returned 0x" << std::hex << ret << "." << std::endl;
            udsDestroyNetwork();
            udsUnbind(&bindctx);
            return;
        }

        tmp = 0;
        ret = udsGetChannel((u8*)&tmp);//Normally you don't need to use this.
        std::cout << "udsGetChannel() returned 0x" << std::hex << ret << ". channel = " << tmp << "." << std::endl;
        if(R_FAILED(ret))
        {
            udsDestroyNetwork();
            udsUnbind(&bindctx);
            return;
        }

        con_type = 0;
    }

    if(udsWaitConnectionStatusEvent(false, false))
    {
        std::cout << "Constatus event signaled." << std::endl;
        print_constatus();
    }

    std::cout << "Press A to stop data transfer." << std::endl;

    tmpbuf_size = UDS_DATAFRAME_MAXSIZE;
    tmpbuf = static_cast<u32*>(malloc(tmpbuf_size));
    if(tmpbuf==NULL)
    {
        std::cout << "Failed to allocate tmpbuf for receiving data." << std::endl;

        if(con_type)
        {
            udsDestroyNetwork();
        }
        else
        {
            udsDisconnectNetwork();
        }
        udsUnbind(&bindctx);

        return;
    }

    while(1)
    {
        gspWaitForVBlank();
        hidScanInput();
        u32 kDown = hidKeysDown();

        if(kDown & KEY_A) break;
        prev_transfer_data = transfer_data;
        transfer_data = hidKeysHeld();

        //When the output from hidKeysHeld() changes, send it over the network.
        if(transfer_data != prev_transfer_data && conntype!=UDSCONTYPE_Spectator)//Spectators aren't allowed to send data.
        {
            ret = udsSendTo(UDS_BROADCAST_NETWORKNODEID, data_channel, UDS_SENDFLAG_Default, &transfer_data, sizeof(transfer_data));
            if(UDS_CHECK_SENDTO_FATALERROR(ret))
            {
                std::cout << "udsSendTo() returned 0x" << std::hex << ret << "." << std::endl;
                break;
            }
        }

        //if(udsWaitDataAvailable(&bindctx, false, false))//Check whether data is available via udsPullPacket().
        {
            memset(tmpbuf, 0, tmpbuf_size);
            actual_size = 0;
            src_NetworkNodeID = 0;
            ret = udsPullPacket(&bindctx, tmpbuf, tmpbuf_size, &actual_size, &src_NetworkNodeID);
            if(R_FAILED(ret))
            {
                std::cout << "udsPullPacket() returned 0x" << std::hex << ret << "." << std::endl;
                break;
            }

            if(actual_size)//If no data frame is available, udsPullPacket() will return actual_size=0.
            {
                std::cout << "Received 0x" << std::hex << tmpbuf[0] << " size=0x" << std::hex << actual_size << " from node 0x" << std::hex << src_NetworkNodeID << "." << std::endl;
            }
        }

        if(kDown & KEY_Y)
        {
            ret = udsGetNodeInformation(0x2, &tmpnode);//This can be used to get the NodeInfo for a node which just connected, for example.
            if(R_FAILED(ret))
            {
                std::cout << "udsGetNodeInformation() returned 0x" << std::hex << ret << "." << std::endl;
            }
            else
            {
                memset(tmpstr, 0, sizeof(tmpstr));

                ret = udsGetNodeInfoUsername(&tmpnode, tmpstr);
                if(R_FAILED(ret))
                {
                    std::cout << "udsGetNodeInfoUsername() returned 0x" << std::hex << ret << " for udsGetNodeInfoUsername." << std::endl;
                }
                else
                {
                    std::cout << "node username: " << tmpstr << std::endl;
                    std::cout << "node unk_x1c=0x" << std::hex << tmpnode.unk_x1c << std::endl;
                    std::cout << "node flag=0x" << std::hex << tmpnode.flag << std::endl;
                    std::cout << "node pad_x1f=0x" << std::hex << tmpnode.pad_x1f << std::endl;
                    std::cout << "node NetworkNodeID=0x" << std::hex << tmpnode.NetworkNodeID << std::endl;
                    std::cout << "node word_x24=0x" << std::hex << tmpnode.word_x24 << std::endl;
                }
            }
        }

        if(kDown & KEY_X)//Block new regular clients from connecting.
        {
            ret = udsSetNewConnectionsBlocked(true, true, false);
            std::cout << "udsSetNewConnectionsBlocked() for enabling blocking returned 0x" << std::hex << ret << "." << std::endl;
        }

        if(kDown & KEY_B)//Unblock new regular clients from connecting.
        {
            ret = udsSetNewConnectionsBlocked(false, true, false);
            std::cout << "udsSetNewConnectionsBlocked() for disabling blocking returned 0x" << std::hex << ret << "." << std::endl;
        }

        if(kDown & KEY_R)
        {
            ret = udsEjectSpectator();
            std::cout << "udsEjectSpectator() returned 0x" << std::hex << ret << "." << std::endl;
        }

        if(kDown & KEY_L)
        {
            ret = udsAllowSpectators();
            std::cout << "udsAllowSpectators() returned 0x" << std::hex << ret << "." << std::endl;
        }

        if(udsWaitConnectionStatusEvent(false, false))
        {
            std::cout << "Constatus event signaled." << std::endl;
            print_constatus();
        }
    }

    free(tmpbuf);
    tmpbuf = NULL;

    if(con_type)
    {
        udsDestroyNetwork();
    }
    else
    {
        udsDisconnectNetwork();
    }
    udsUnbind(&bindctx);
}

int main()
{
    Result ret=0;

    gfxInitDefault();
    consoleInit(GFX_TOP, NULL);

    std::cout << "libctru UDS local-WLAN demo." << std::endl;

    ret = udsInit(0x3000, NULL);//The sharedmem size only needs to be slightly larger than the total recv_buffer_size for all binds, with page-alignment.
    if(R_FAILED(ret))
    {
        std::cout << "udsInit failed: 0x" << std::hex << ret << "." << std::endl;
    }
    else
    {
        uds_test();
        udsExit();
    }

    std::cout << "Press START to exit." << std::endl;

    // Main loop
    while (aptMainLoop())
    {
        gspWaitForVBlank();
        hidScanInput();

        u32 kDown = hidKeysDown();
        if (kDown & KEY_START)
            break; // break in order to return to hbmenu

        // Flush and swap framebuffers
        gfxFlushBuffers();
        gfxSwapBuffers();
    }

    gfxExit();
    return 0;
}

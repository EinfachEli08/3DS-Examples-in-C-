#include <iostream>
#include <cstring>
#include <memory>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <3ds.h>
#include "builtin_rootca_der.h"

static char http_netreq[] = "GET /testpage HTTP/1.1\r\nUser-Agent: 3ds-examples_sslc\r\nConnection: close\r\nHost: yls8.mtheall.com\r\n\r\n";

char readbuf[0x400];

void network_request(const char *hostname) {
    Result ret = 0;

    struct addrinfo hints;
    struct addrinfo *resaddr = nullptr, *resaddr_cur;
    int sockfd;

    sslcContext sslc_context;
    u32 RootCertChain_contexthandle = 0;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        std::cout << "Failed to create the socket." << std::endl;
        return;
    }

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    std::cout << "Resolving hostname..." << std::endl;

    if (getaddrinfo(hostname, "443", &hints, &resaddr) != 0) {
        std::cout << "getaddrinfo() failed." << std::endl;
        close(sockfd);
        return;
    }

    std::cout << "Connecting to the server..." << std::endl;

    for (resaddr_cur = resaddr; resaddr_cur != nullptr; resaddr_cur = resaddr_cur->ai_next) {
        if (connect(sockfd, resaddr_cur->ai_addr, resaddr_cur->ai_addrlen) == 0) break;
    }

    freeaddrinfo(resaddr);

    if (resaddr_cur == nullptr) {
        std::cout << "Failed to connect." << std::endl;
        close(sockfd);
        return;
    }

    std::cout << "Running sslc setup..." << std::endl;

    ret = sslcCreateRootCertChain(&RootCertChain_contexthandle);
    if (R_FAILED(ret)) {
        std::cout << "sslcCreateRootCertChain() failed: 0x" << std::hex << ret << std::endl;
        close(sockfd);
        return;
    }

    ret = sslcAddTrustedRootCA(RootCertChain_contexthandle, (u8 *)builtin_rootca_der, builtin_rootca_der_size, nullptr);
    if (R_FAILED(ret)) {
        std::cout << "sslcAddTrustedRootCA() failed: 0x" << std::hex << ret << std::endl;
        close(sockfd);
        sslcDestroyRootCertChain(RootCertChain_contexthandle);
        return;
    }

    ret = sslcCreateContext(&sslc_context, sockfd, SSLCOPT_Default, hostname);
    if (R_FAILED(ret)) {
        std::cout << "sslcCreateContext() failed: 0x" << std::hex << ret << std::endl;
        close(sockfd);
        sslcDestroyRootCertChain(RootCertChain_contexthandle);
        return;
    }

    ret = sslcContextSetRootCertChain(&sslc_context, RootCertChain_contexthandle);
    if (R_FAILED(ret)) {
        std::cout << "sslcContextSetRootCertChain() failed: 0x" << std::hex << ret << std::endl;
        sslcDestroyContext(&sslc_context);
        sslcDestroyRootCertChain(RootCertChain_contexthandle);
        close(sockfd);
        return;
    }

    std::cout << "Starting the TLS connection..." << std::endl;

    ret = sslcStartConnection(&sslc_context, nullptr, nullptr);
    if (R_FAILED(ret)) {
        std::cout << "sslcStartConnection() failed: 0x" << std::hex << ret << std::endl;
        sslcDestroyContext(&sslc_context);
        sslcDestroyRootCertChain(RootCertChain_contexthandle);
        close(sockfd);
        return;
    }

    std::cout << "Sending request..." << std::endl;

    ret = sslcWrite(&sslc_context, (u8 *)http_netreq, strlen(http_netreq));
    if (R_FAILED(ret)) {
        std::cout << "sslcWrite() failed: 0x" << std::hex << ret << std::endl;
        sslcDestroyContext(&sslc_context);
        sslcDestroyRootCertChain(RootCertChain_contexthandle);
        close(sockfd);
        return;
    }

    std::cout << "Total sent size: 0x" << std::hex << ret << std::endl;

    memset(readbuf, 0, sizeof(readbuf));

    ret = sslcRead(&sslc_context, readbuf, sizeof(readbuf) - 1, false);
    if (R_FAILED(ret)) {
        std::cout << "sslcWrite() failed: 0x" << std::hex << ret << std::endl;
        sslcDestroyContext(&sslc_context);
        sslcDestroyRootCertChain(RootCertChain_contexthandle);
        close(sockfd);
        return;
    }

    std::cout << "Total received size: 0x" << std::hex << ret << std::endl;

    std::cout << "Reply:" << std::endl << readbuf << std::endl;

    sslcDestroyContext(&sslc_context);
    sslcDestroyRootCertChain(RootCertChain_contexthandle);

    close(sockfd);
}

int main() {
    Result ret = 0;
    u32 *soc_sharedmem;
    u32 soc_sharedmem_size = 0x100000;

    gfxInitDefault();
    consoleInit(GFX_TOP, nullptr);

    std::cout << "libctru sslc demo." << std::endl;

    soc_sharedmem = static_cast<u32 *>(aligned_alloc(0x1000, soc_sharedmem_size));

    if (soc_sharedmem == nullptr) {
        std::cout << "Failed to allocate SOC sharedmem." << std::endl;
    } else {
        ret = socInit(soc_sharedmem, soc_sharedmem_size);

        if (R_FAILED(ret)) {
            std::cout << "socInit failed: 0x" << std::hex << ret << std::endl;
        } else {
            ret = sslcInit(0);
            if (R_FAILED(ret)) {
                std::cout << "sslcInit failed: 0x" << std::hex << ret << std::endl;
            } else {
                network_request("yls8.mtheall.com");
                sslcExit();
            }

            socExit();
        }
    }

    std::cout << "Press START to exit." << std::endl;

    // Main loop
    while (aptMainLoop()) {
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

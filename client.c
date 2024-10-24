//choosing encoding
#undef UTF8

//exclude unnecessary headers
#define WIN32_LEAN_AND_MEAN

//general includes
#include <stdio.h>

//winapi and socket includes
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

//linking libs
#pragma comment(lib, "Ws2_32.lib")

//socket port
#define DEFAULT_PORT "55555"
//len of pipe buf
#define BUFSIZE 512

//my functions declaration
//maybe bad naming, it is symmetric to server side in socket interaction
//but of course there are no any child processes in client (and obviously it is not pipes between processes)
//so "pipes" in these names are just "pipes" between client and server via sockets
DWORD WINAPI write_pipe(LPVOID); //send command to server
DWORD WINAPI read_pipe(LPVOID); //get response from server

//blank socket
SOCKET ConnectionSocket = INVALID_SOCKET;


//client is simplier a bit than a server, so code here can be not so "serious"
//for example, easier reaction to returned error values of system functions


int main(int argc, char* argv[]) {
    //need 1 argument of server address, for example:
    //client.exe 192.168.1.75
    printf("\nPlease, type exit and press enter three times (until send error) for normal disconnection\nDON NOT USE ctrl+c in order not to break server!\n\n");
    if (argc != 2) {
        printf("Need only one command line argument (server address), try again!\nExample: client.exe 192.168.1.75\n");
        return 1;
    }
    //working with socket is same as in server
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup error: %d\n", WSAGetLastError());
        return 1;
    }
    struct addrinfo *result = NULL, *ptr = NULL, hints;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    int iResult = getaddrinfo(argv[1], DEFAULT_PORT, &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo error: %d\n", iResult);
        WSACleanup();
        return 1;
    }
    //trying all possibilities (getted by getaddrinfo) to connect to server
    for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {
        ConnectionSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (ConnectionSocket == INVALID_SOCKET) {
            printf("socket error: %d\n", WSAGetLastError());
            WSACleanup();
            return 1;
        }
        iResult = connect(ConnectionSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
            closesocket(ConnectionSocket);
            ConnectionSocket = INVALID_SOCKET;
            continue;
        }
        break;
    }
    freeaddrinfo(result);
    if (ConnectionSocket == INVALID_SOCKET) {
        printf("Cannot connect to server! Recheck address of server maybe)))\n");
        WSACleanup();
        return 1;
    }
    //this part is also symmetric to server side
    HANDLE pipeThreads[2];
    pipeThreads[0] = CreateThread(NULL, 0, write_pipe, NULL, 0, NULL);
    pipeThreads[1] = CreateThread(NULL, 0, read_pipe, NULL, 0, NULL);
    WaitForMultipleObjects(2, pipeThreads, TRUE, INFINITE);
    iResult = shutdown(ConnectionSocket, SD_SEND);
    if (iResult == SOCKET_ERROR) {
        printf("shutdown error: %d\n", WSAGetLastError());
    }
    closesocket(ConnectionSocket);
    WSACleanup();
    return 0;
}

DWORD WINAPI write_pipe(LPVOID lpParam) {
    UNREFERENCED_PARAMETER(lpParam);
    DWORD iResult, dwRead, bSuccess;
    //this part is also symmetric to server side
    for (;;) {
        CHAR buffer[BUFSIZE] = {0};
        bSuccess = ReadFile(GetStdHandle(STD_INPUT_HANDLE), buffer, BUFSIZE - 1, &dwRead, NULL);
        if (!bSuccess || dwRead == 0) {
            return -1;
        }
        buffer[dwRead] = '\0';
        iResult = send(ConnectionSocket, buffer, dwRead, 0);
        if (iResult == SOCKET_ERROR) {
            printf("send error\n");
            return -1;
        }
    }
}

DWORD WINAPI read_pipe(LPVOID lpParam) {
    UNREFERENCED_PARAMETER(lpParam);
    int iResult, bSuccess;
    char buffer[BUFSIZE];
    //this part is also symmetric to server side
    for (;;) {
        iResult = recv(ConnectionSocket, buffer, BUFSIZE - 1, 0);
        if (iResult == 0) {
            return 0;
        }
        if (iResult > 0) {
            buffer[iResult] = '\0';
            bSuccess = WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buffer, iResult, NULL, NULL);
            if (!bSuccess) {
                return -1;
            }
        }
        else {
            printf("recv error\n");
            return -1;
        }
    }
}

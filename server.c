//choosing encoding
#undef UTF8

//exclude unnecessary headers
#define WIN32_LEAN_AND_MEAN

//general includes
#include <stdio.h>
#include <stdlib.h>

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

//blank socket
SOCKET ClientSocket = INVALID_SOCKET;
//pipe handles 
HANDLE g_hChildStd_IN_Rd = NULL;
HANDLE g_hChildStd_IN_Wr = NULL;
HANDLE g_hChildStd_OUT_Rd = NULL;
HANDLE g_hChildStd_OUT_Wr = NULL;

//my functions declaration
void create_pipes(void);
void create_socket(void);
void create_child_process(void);
void close_connection(void);
DWORD WINAPI write_pipe(LPVOID); //writing to pipe
DWORD WINAPI read_pipe(LPVOID); //reading from pipe

int main() {
    for (;;) {
        //init all resources
        create_pipes();
        create_socket();
        create_child_process();
        //writing and reading threads
        HANDLE threads[2];
        threads[0] = CreateThread(NULL, 0, write_pipe, NULL, 0, NULL);
        threads[1] = CreateThread(NULL, 0, read_pipe, NULL, 0, NULL);
        //wait for finishing of client working and closing server resources
        //!!!CLIENT MUST TYPE exit FOR NORMAL REMOTE CMD CLOSING and only AFTER press ctrl+c
        //so next client connection will be available without server manual restart
        WaitForMultipleObjects(2, threads, TRUE, INFINITE);
        for (int i = 0; i < 2; i++) {
            CloseHandle(threads[i]);
        }
        close_connection();
    }
    return 0;
}

void create_pipes(void) {
    //security attributes struct
    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;
    //out pipe creation
    if (!CreatePipe(&g_hChildStd_OUT_Rd, &g_hChildStd_OUT_Wr, &saAttr, 0) || !SetHandleInformation(g_hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0)) {
        fprintf(stderr, "create out pipe error\n");
        ExitProcess(1);
    }
    //in pipe creation
    if (!CreatePipe(&g_hChildStd_IN_Rd, &g_hChildStd_IN_Wr, &saAttr, 0) || !SetHandleInformation(g_hChildStd_IN_Wr, HANDLE_FLAG_INHERIT, 0)) {
        fprintf(stderr, "create in pipe error\n");
        ExitProcess(1);
    }
}

void create_socket(void) {
    //socket characteristics struct
    WSADATA wsaData;
    //var for error checking
    int iResult;
    //init winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        fprintf(stderr, "WSAStartup error: %d\n", iResult);
        ExitProcess(1);
    }
    //resolving server address and port
    struct addrinfo *result = NULL;
    struct addrinfo hints;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;
    iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
    if (iResult != 0) {
        fprintf(stderr, "getaddrinfo error: %d\n", iResult);
        WSACleanup();
        ExitProcess(1);
    }
    //new socket
    SOCKET ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (ListenSocket == INVALID_SOCKET) {
        fprintf(stderr, "socket error: %d\n", WSAGetLastError());
        freeaddrinfo(result);
        WSACleanup();
        ExitProcess(1);
    }
    //bind socket
    iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        fprintf(stderr, "bind error: %d\n", WSAGetLastError());
        freeaddrinfo(result);
        closesocket(ListenSocket);
        WSACleanup();
        ExitProcess(1);
    }
    freeaddrinfo(result);
    //listen socket
    iResult = listen(ListenSocket, SOMAXCONN);
    if (iResult == SOCKET_ERROR) {
        fprintf(stderr, "listen error: %d\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
        ExitProcess(1);
    }
    //accept client socket
    ClientSocket = accept(ListenSocket, NULL, NULL);
    if (ClientSocket == INVALID_SOCKET) {
        fprintf(stderr, "accept error: %d\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
        ExitProcess(1);
    }
    //and close server socket
    closesocket(ListenSocket);
}

void create_child_process(void) {
    TCHAR szCmdline[] = TEXT("C:\\Windows\\System32\\cmd.exe"); //we want to make remote console
    PROCESS_INFORMATION piProcInfo;
    STARTUPINFO siStartInfo;
    ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));
    ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
    siStartInfo.cb = sizeof(STARTUPINFO);
    siStartInfo.hStdError = g_hChildStd_OUT_Wr;
    siStartInfo.hStdOutput = g_hChildStd_OUT_Wr;
    siStartInfo.hStdInput = g_hChildStd_IN_Rd;
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES;
    //process creation
    if (!CreateProcess(NULL, szCmdline, NULL, NULL, TRUE, 0, NULL, NULL, &siStartInfo, &piProcInfo)) {
        fprintf(stderr, "create child error\n");
        ExitProcess(1);
    } else {
        CloseHandle(piProcInfo.hProcess);
        CloseHandle(piProcInfo.hThread);
        CloseHandle(g_hChildStd_OUT_Wr);
        CloseHandle(g_hChildStd_IN_Rd);
    }
}

DWORD WINAPI write_pipe(LPVOID lpParam) {
    UNREFERENCED_PARAMETER(lpParam);
    DWORD dwWritten, iResult;
    CHAR chBuf[BUFSIZE];
    //receive commands from client socket and write to child (cmd) pipe
    for (;;) {
        iResult = recv(ClientSocket, chBuf, BUFSIZE, 0);
        if (iResult == 0) {
            return 0;
        }
        if (iResult == SOCKET_ERROR) {
            fprintf(stderr, "recv error\n");
            exit(-1);
        }
        if (!WriteFile(g_hChildStd_IN_Wr, chBuf, iResult, &dwWritten, NULL) || dwWritten == 0) {
            return 0;
        }
    }
    return 0;
}

DWORD WINAPI read_pipe(LPVOID lpParam) {
    UNREFERENCED_PARAMETER(lpParam);
    DWORD dwRead, iResult;
    CHAR chBuf[BUFSIZE + 1];
    //read from child (cmd) pipe response and send to client
    for (;;) {
        if (!ReadFile(g_hChildStd_OUT_Rd, chBuf, BUFSIZE, &dwRead, NULL)) {
            return 0;
        }
        chBuf[dwRead] = '\0';
        iResult = send(ClientSocket, chBuf, dwRead, 0);
        if (iResult == SOCKET_ERROR) {
            fprintf(stderr, "send error\n");
            exit(-1);
        }
    }
    return 0;
}

void close_connection(void) {
    //shutdown socket
    int iResult = shutdown(ClientSocket, SD_SEND);
    if (iResult == SOCKET_ERROR) {
        fprintf(stderr, "shutdown error: %d\n", WSAGetLastError());
        closesocket(ClientSocket);
        WSACleanup();
        ExitProcess(1);
    }
    //free resources
    closesocket(ClientSocket);
    WSACleanup();
}

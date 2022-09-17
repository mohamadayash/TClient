#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#define _CRT_RAND_S

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <vector>
#include <string>
#include <mutex>
#include <fstream>
#include <ctime>
#include <sstream>






using namespace std;

//class CommandType {
//public:
//    static inline const string LOGIN="L0";
//    static const string TRADE;
//};


// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")


#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "5555"

#define LOGIN "L0"
#define TRADE "T0"

const char* DELIMITER = " ";
const char BREAK = '\r';

string filename("trades.log");

PCSTR serverip = "192.168.0.121";
SOCKET ConnectSocket = INVALID_SOCKET;

std::mutex mtxsend;
std::mutex mtxlog;

void joinCommandParts(const vector<string>& v, string& s) {
    s.clear();
    for (vector<string>::const_iterator p = v.begin();
        p != v.end(); ++p) {
        s += *p;
        if (p != v.end() - 1)
            s += DELIMITER;
        else
            s += "\n";
    }
}

bool InitializeAndConnect() {
    WSADATA wsaData;
    struct addrinfo* result = NULL,
        * ptr = NULL,
        hints;


    int iResult;

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return false;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    // Resolve the server address and port
    iResult = getaddrinfo(serverip, DEFAULT_PORT, &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        return false; 
    }

    // Attempt to connect to an address until one succeeds
    for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {

        // Create a SOCKET for connecting to server
        ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype,
            ptr->ai_protocol);
        if (ConnectSocket == INVALID_SOCKET) {
            printf("socket failed with error: %ld\n", WSAGetLastError());
            WSACleanup();
            return false;
        }

        // Connect to server.
        iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
            closesocket(ConnectSocket);
            ConnectSocket = INVALID_SOCKET;
            continue;
        }
        break;
    }

    freeaddrinfo(result);

    if (ConnectSocket == INVALID_SOCKET) {
        printf("Unable to connect to server!\n");
        WSACleanup();
        return false;
    }

    return true;
}

void logTrade(string command) {
    mtxlog.lock();
    std::ofstream file_out;
    file_out.open(filename, std::ios_base::app);
    file_out << command;
    mtxlog.unlock();
}

bool send(string command) {

    mtxsend.lock();

    int iResult = send(ConnectSocket, command.c_str(),command.length(), 0);

    if (iResult == SOCKET_ERROR) {
        printf("send failed with error: %d\n", WSAGetLastError());
        closesocket(ConnectSocket);
        WSACleanup();
        mtxsend.unlock();
        return false;
    }

    printf("command sent:%s", command.c_str());
    mtxsend.unlock();
    return true;
}


void sendTrade() {

    while (true) {
        unsigned int number;
        rand_s(&number);

        srand(number);

        vector<string> symbols = { "AUDCAD", "AUDCHF", "AUDJPY", "AUDNZD", "AUDUSD", "CADCHF", "CADJPY" };
        int randIndex = rand() % 6;

        long login = rand() % (2000 - 1000 + 1) + 1000;
        long deal = rand() % (50000 - 10000 + 1) + 10000;
        int action = rand() % 2;
        string symbol = symbols.at(randIndex);
        double price = 1.2030 + (double)rand() * (100.50 - 1.2030) / (double)RAND_MAX;
        double profit = -5000.0 + (double)rand() * (10000.0 + 5000.0) / (double)RAND_MAX;
        long volume = rand() % (100 - 1 + 1) + 1;

        std::time_t curr_time = std::time(nullptr);
        std::stringstream ss;
        ss << curr_time;
        std::string ts = ss.str();

        vector<string> commparts = { TRADE,to_string(login),to_string(deal),to_string(action),symbol,to_string(price),to_string(profit),to_string(volume),ts };

        string command;

        joinCommandParts(commparts, command);

        bool res = send(command.c_str());

        if (res == true) {
            logTrade(command);
        }
        
         std::this_thread::sleep_for(100ms);
    }
}

bool sendLogin(string username,string password) {
    //build command
    vector<string> commparts = {LOGIN,username,password };
    string command;
    joinCommandParts(commparts, command);
    return send(command.c_str());
}

void handleLogin(vector<char*> parameters) {

    char* loginResult = parameters.at(0);

    if (strcmp(loginResult, "1") == 0) {
        printf("successful login.\n");
        //prepair threads 
        for (int i = 0; i < 1000; i++) {
            DWORD threadId;
            HANDLE listenThread = CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)sendTrade,NULL,0,&threadId);
        }
    }
    else {
        printf("login failed.\n");
    }
}

char* parseCommand(char* buffer,vector<char*>* parameters) {
    vector<char*> argTokens;
    char* tokenInBuffer;
    //split
    char* commandType = strtok_s(buffer, DELIMITER, &tokenInBuffer);
    char* currentValue = NULL;
    do {
        currentValue = strtok_s(NULL, " ", &tokenInBuffer);
        if (currentValue == NULL)break;
        (*parameters).push_back(currentValue);
    } while (currentValue != NULL);

    return commandType;
}

void receive() {
    int iResult;
    std::string command = "";
    do {
        char* buff = new char[1];
        iResult = recv(ConnectSocket, buff, 1, 0);
        if (iResult > 0) {
            char value = *buff;
            if (value == BREAK) {
                vector<char*> parameters;
                char* commandType = parseCommand((char*)command.c_str(),&parameters);

                printf("command received: %s\n", command.c_str());
                
                if (strcmp(commandType, LOGIN) == 0) {
                    handleLogin(parameters);
                }

                command.clear();
            }
            else {
                command += value;
            }
        }
        else if (iResult == 0)
            printf("Connection closed\n");
        else
            printf("recv failed with error: %d\n", WSAGetLastError());

    } while (iResult > 0);

    // cleanup
    closesocket(ConnectSocket);
    WSACleanup();
}

bool shutdown() {
    int iResult = shutdown(ConnectSocket, SD_SEND);
    if (iResult == SOCKET_ERROR) {
        printf("shutdown failed with error: %d\n", WSAGetLastError());
        closesocket(ConnectSocket);
        WSACleanup();
        return false;
    }
    return true;
}

void main()
{
    bool res = InitializeAndConnect();

    if (res) {

        //start receiving

        DWORD threadId;

        HANDLE listenThread = CreateThread(
            NULL,                                  // default security attributes
            0,                                     // use default stack size  
            (LPTHREAD_START_ROUTINE)receive,       // thread function name
            NULL,                        // argument to thread function 
            0,
            &threadId);
        
        string username;
        string password;

        cout << "enter username:\n";
        cin >> username;

        cout << "enter password:\n";
        cin >> password;

        sendLogin(username, password);

        while (true);
    }
}
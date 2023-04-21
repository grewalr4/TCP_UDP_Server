/*
Name: Ravdeep Grewal
Date: 9/16/22
CSE 422 Project 1 Server Code
*/

#include <cstdio>
#include <cstdlib>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>

#include <iostream>
#include <sstream>
#include <cstring>
#include <unistd.h>

#include "packet.h" // defined by us
#include "Bulls_And_Cows.h"
#include "project1_server.h"

using namespace std;

const int BUFLEN = 256;

// Function prototypes
bool check_bytes(int bytes, My_Packet& packet, bool sending, bool tcp);
int tcp_init();
int udp_init(unsigned short int& udp_server_port);
void comm_handler(int& tcp_client_fd);

int main(int argc, char* argv[])
{
    parse_argv(argc, argv);

    cout << "[SYS] Parent process for TCP communication." << endl; 
    cout << "[TCP] Bulls and Cows game server started..." << endl;

    int tcp_server_fd;  // file descriptor for the server TCP socket

    sockaddr_in tcp_client_addr; // process client socket addr
    socklen_t tcp_client_addr_len = sizeof(tcp_client_addr);
    int tcp_client_fd;  // file descriptor for the client TCP socket

    int bytes_recv; 
    int bytes_sent;
    int pid = 0;  // placeholder for pid

    tcp_server_fd = tcp_init();  // initialize the TCP server

    // listen to socket, wait for incoming connections
    listen(tcp_server_fd, 1);

    while (1)
    {
        tcp_client_fd = accept(tcp_server_fd, (struct sockaddr *) &tcp_client_addr, &tcp_client_addr_len);  // accept a connection over TCP
        if (tcp_client_fd < 0)  // there was an error
        {
            cerr << "[ERR] Error accepting client" << endl;
        }

        pid = fork();  // creating child process

        if (pid < 0)  // fork was unsuccessful
        {
            cerr << "[ERR] fork() failed" << endl;
            exit(1);
        }
        else if (pid == 0)  // handle the gameplay inside the child process with dedicated UDP interactions
        {
            cout << "[SYS] Child process forked" << endl;
            comm_handler(tcp_client_fd); 
            break; 
        }
        
    }

    if (pid == 0) // log to console that the child process is finished
    {
        cout << "[SYS] child process terminated" << endl;
    }

    return 0;
}

/**
* Creates a TCP server and returns connecting socket
* @return TCP socket fd
*/
int tcp_init()
{
    unsigned short int tcp_server_port;  // TCP port
    sockaddr_in tcp_server_addr;    
    socklen_t tcp_server_addr_len = sizeof(tcp_server_addr);
    int tcp_server_fd; 

    // create TCP socket to listen for client connections
    tcp_server_fd = socket(AF_INET, SOCK_STREAM, 0);

    // error checking for creation of socket
    if (tcp_server_fd < 0)
    {
        cerr << "[ERR] Cannot create the TCP socket" << endl;
        exit(1);
    }

    // init socket address
    memset(&tcp_server_addr, 0, sizeof(tcp_server_addr));

    // setting server attributes
    tcp_server_addr.sin_family = AF_INET;
    tcp_server_addr.sin_port = 0;
    tcp_server_addr.sin_addr.s_addr = INADDR_ANY;

    // set socket address
    if (bind(tcp_server_fd, (sockaddr *)&tcp_server_addr, sizeof(tcp_server_addr)) < 0)
    {
        cerr << "[ERR] Cannot bind the TCP socket" << endl;
        exit(1);
    }
    
    // getting socket name
    if (getsockname(tcp_server_fd, (struct sockaddr *) &tcp_server_addr, &tcp_server_addr_len) < 0)
    {
        cerr << "[ERR] Cannot get the TCP socket" << endl;
        exit(1);
    }
    tcp_server_port = ntohs(tcp_server_addr.sin_port);  // port translation

    cout << "[TCP] port: " << tcp_server_port << endl;

    return tcp_server_fd;
}

/**
* Creates a UDP server and returns connecting socket
* Updates udp_server_port for gameplay
* @param udp_server_port UDP server port to fetch
* @return UDP socket fd
*/
int udp_init(unsigned short int& udp_server_port)
{
    udp_server_port = 0; // reset value to 0
    sockaddr_in udp_server_addr;
    socklen_t udp_server_addr_len = sizeof(udp_server_addr);
    int udp_server_fd;      

    // create UDP socket to listen for client connections
    udp_server_fd = socket(AF_INET, SOCK_DGRAM, 0);
  
    // error checking for creation of socket
    if (udp_server_fd < 0)
    {
        cerr << "[ERR] Cannot create the UDP socket" << endl;
        exit(1);
    }

    // init socket address
    memset(&udp_server_addr, 0, sizeof(udp_server_addr));

    // setting server attributes
    udp_server_addr.sin_family = AF_INET;   
    udp_server_addr.sin_port = 0;     
    udp_server_addr.sin_addr.s_addr = INADDR_ANY;

    // binding socket to address
    if (bind(udp_server_fd, (sockaddr *) &udp_server_addr, sizeof(udp_server_addr)) < 0)
    {
        cerr << "[ERR] Cannot bind the UDP socket." << endl;
        exit(1);
    }

    // getting socket name
    if (getsockname(udp_server_fd, (struct sockaddr *) &udp_server_addr, &udp_server_addr_len) < 0)
    {
        cerr << "[ERR] Cannot get the UDP socket" << endl;
        exit(1);
    }
    udp_server_port = ntohs(udp_server_addr.sin_port);  // port translation

    return udp_server_fd;
}

/**
* Handle communication for gameplay
* @param tcp_client_fd TCP client fd
*/
void comm_handler(int& tcp_client_fd)
{
    sockaddr_in udp_client_addr;
    socklen_t udp_client_addr_len = sizeof(udp_client_addr);

    string message = "";
    int bytes_recv;
    int bytes_sent;

    My_Packet inc_pkt;
    My_Packet out_pkt;
    char type_name[type_name_len];

    memset(&inc_pkt, 0, sizeof(inc_pkt));  // clear incoming packet before receiving

    // Receive the, hopefully, JOIN message
    bytes_recv = recv(tcp_client_fd, &inc_pkt, sizeof(inc_pkt), 0);
    if (!check_bytes(bytes_recv, inc_pkt, false, true))  // error checking bytes received
    {
        exit(1);
    }

    if (inc_pkt.type == JOIN)  // JOIN handling sets up the UDP server
    {
        unsigned short int udp_server_port = -1;  // port to send GUESS to
        int udp_server_fd = udp_init(udp_server_port);

        if (udp_server_port == -1)  // UDP port not properly set
        {
            cerr << "[ERR] Issue when establishing UDP port" << endl;
            exit(1);
        }
        cout << "[UDP:" << udp_server_port << "] Gameplay server started" << endl;

        // setup the outgoing packet
        out_pkt.type = JOIN_GRANT;
        memcpy(out_pkt.buffer, to_string(udp_server_port).c_str(), sizeof(out_pkt.buffer));

        // Respond with a JOIN_GRANT packet
        bytes_sent = send(tcp_client_fd, &out_pkt, sizeof(out_pkt), 0);
        if (!check_bytes(bytes_recv, inc_pkt, true, true))  // error checking bytes sent
        {
            exit(1);
        }

        // listen to UDP socket
        listen(udp_server_fd, 1);
 
        if (close(tcp_client_fd) < 0) // close tcp client socket because it's no longer in use
        {
            cerr << "[ERR] Error closing TCP socket" << endl;
            exit(1);
        }  

        Bulls_And_Cows active_game;  // the active game
        bool end_state = true;  // sentinel tracking game progress
        int secret_number;  // gameplay number
        
        while(1)  // gameplay loop
        {
            if (end_state)  // setup the game -- runs by default
            {
                active_game.Restart_Game();
                secret_number = -1;
                active_game.get_secret_number(secret_number);
                string delimit = "";

                if (secret_number < 0)
                {
                    cerr << "[ERR] Error generating secret number" << endl;
                    exit(1);
                }

                if (secret_number < 1000)
                {
                    delimit = "0";  // add missing zero
                }

                cout << "[UDP:" << udp_server_port << "] A new game has started." << endl;
                cout << "[UDP:" << udp_server_port << "] Secret number: " << delimit << secret_number << endl;
            }

            memset(&inc_pkt, 0, sizeof(inc_pkt));  // clear before receiving
            memset(&out_pkt, 0, sizeof(out_pkt));  // clear before sending

            bytes_recv = recvfrom(udp_server_fd, &inc_pkt, sizeof(inc_pkt), 0, 
                                      (sockaddr *) &udp_client_addr, 
                                      &udp_client_addr_len);  // receiving message from client

            if (!check_bytes(bytes_recv, inc_pkt, false, false))  // error checking bytes received
            {
                exit(1);
            }

            // UDP Sending/Receiving interactions
            if (inc_pkt.type == EXIT)  // is this an EXIT?
            {
                out_pkt.type = EXIT_GRANT;
                message = "Exit granted, goodbye.";
            }
            else if (inc_pkt.type == GUESS)  // is this a GUESS?
            {
                out_pkt.type = RESPONSE;
                char guess[game_len];  // buffer to hold guess
                int bulls = 0;  // guess digits in correct place
                int cows = 0;  // guess digits that are found in the secret number

                memcpy(&guess, inc_pkt.buffer, game_len);
                end_state = active_game.Guess(guess, bulls, cows);  // determining if game is over

                message = to_string(bulls) + "A" + to_string(cows) + "B";
            }
            else
            {
                cerr << "[ERR] Unknown packet type" << endl;
            }
            
            // preparing to send message
            memcpy(out_pkt.buffer, message.c_str(), sizeof(out_pkt.buffer));
            bytes_sent = sendto(udp_server_fd, &out_pkt, sizeof(out_pkt), 0, (sockaddr *) &udp_client_addr, 
                        udp_client_addr_len);  // sending message to client

            if (!check_bytes(bytes_sent, out_pkt, true, false))  // error checking bytes sent
            {
                exit(1);
            }

            if (out_pkt.type == EXIT_GRANT && inc_pkt.type == EXIT)  // if EXIT/EXIT_GRANT handshake done, close UDP gameplay server
            {
                close(udp_server_fd);
                break;
            }
        }
    }
}

/**
* Handle error checking for bytes
* Also logs messages for UDP/TCP receiving/sending
* @param bytes Bytes involved in exchange
* @param packet Packet involved in exchange
* @param sending Sending or receiving? Default=sending
* @param tcp TCP or UDP? Default=TCP
*/
bool check_bytes(int bytes, My_Packet& packet, bool sending=false, bool tcp=false)
{
    string pkt_type = (tcp) ? "TCP" : "UDP";
    string direction = (sending) ? "Sent" : "Recv";
    string verb = (sending) ? "sending" : "receiving";
    if (bytes < 0) 
    {
        cerr << "[ERR] Error " << verb << " message." << endl;
        return false;
    } 
    else 
    {
        char type_name[type_name_len];
        get_type_name(packet.type, type_name);
        cout << "[" << pkt_type << "] " << direction << ": " << type_name
                 << " " << packet.buffer << endl;
        return true;
    }
}

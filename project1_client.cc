/*
Name: Ravdeep Grewal
Date: 9/16/22
CSE 422 Project 1 Client Code
*/

#include <cstdio>
#include <cstdlib>
#include <sys/socket.h>
#include <netinet/in.h>

#include <iostream>
#include <sstream>
#include <cstring>
#include <string>

#include <netdb.h>
#include <unistd.h>

#include "packet.h"      // defined by us
#include "project1_client.h" // some supporting functions.

using namespace std;

bool check_bytes(int bytes,  My_Packet& packet, bool sending, bool tcp);

const int A_LOC = 0;
const int B_LOC = 2;
const int RESPONSE_LEN = 4;

int main(int argc, char *argv[])
{
    char *server_name_str = 0;

    unsigned short int  tcp_server_port;  // TCP port
    sockaddr_in tcp_server_addr;  // server address
    unsigned short int  udp_server_port;  // UDP port
    sockaddr_in udp_server_addr;   

    int udp_server_fd;
    int tcp_client_fd;

    int bytes_recv;
    int bytes_sent;
    char type_name[type_name_len];

    My_Packet inc_pkt;
    My_Packet out_pkt;
    
    // parse the argvs, obtain server_name and tcp_server_port
    parse_argv(argc, argv, &server_name_str, tcp_server_port);

    struct hostent *hostEnd = gethostbyname(server_name_str);

    cout << "[TCP] Bulls and Cows client started..." << endl;
    cout << "[TCP] Connecting to server: " << server_name_str
         << ":" << tcp_server_port << endl;

    memset(&tcp_server_addr, 0, sizeof(tcp_server_addr));

    // setup TCP protocol attributes
    tcp_server_addr.sin_family = AF_INET;
    tcp_server_addr.sin_port   = htons(tcp_server_port);
    memcpy(&tcp_server_addr.sin_addr, hostEnd -> h_addr, hostEnd -> h_length);

    while (1)
    {
        // TCP: CONNECT TO SERVER

        // create a TCP socket
        tcp_client_fd = socket(AF_INET, SOCK_STREAM, 0); 

        if (tcp_client_fd < 0) 
        {
            cerr << "[ERR] Unable to create TCP socket." << endl;
            exit(1);
        }   

        // attempt to establish connection with server
        if (connect(tcp_client_fd, (sockaddr *) &tcp_server_addr, sizeof(tcp_server_addr)) < 0) 
        {
            cerr << "[ERR] Cannot connect to server" << endl;
            if (close(tcp_client_fd))  // if we can't connect, then we need to close this socket and throw an error
            {
                cerr << "[ERR] Error when closing the TCP socket" << endl;
            }
            exit(1);
        }

        // Sending the JOIN message to the server
        memset(&out_pkt, 0, sizeof(out_pkt));
        out_pkt.type = JOIN;
        bytes_sent = send(tcp_client_fd, &out_pkt, sizeof(out_pkt), 0);

        if (!check_bytes(bytes_sent, out_pkt, true, true))
        {
            exit(1);
        }

        // Receiving the JOIN_GRANT message from the server
        memset(&inc_pkt, 0, sizeof(inc_pkt));
        bytes_recv = recv(tcp_client_fd, &inc_pkt, sizeof(inc_pkt), 0);

        if (!check_bytes(bytes_recv, inc_pkt, false, true))  // error checking bytes received
        {
            exit(1);
        }

        if (inc_pkt.type != JOIN_GRANT)
        {
            cerr << "[ERR] JOIN not granted" << endl;
            exit(1);
        }
        else
        {
            udp_server_port = atoi(inc_pkt.buffer);
            break;
        }
    }

    cout << "[UDP] Guesses will be sent to: " << server_name_str 
         << " at port:" << udp_server_port << endl;

    udp_server_fd = socket(AF_INET, SOCK_DGRAM, 0);  // creation of UDP socket for gameplay

    if (udp_server_fd < 0)  // unsuccessful UDP socket creation
    {
        cerr << "[ERR] Cannot create the UDP socket." << endl;
        exit(1);
    }

    // setup the UDP protocol attributes
    udp_server_addr.sin_family = AF_INET;
    udp_server_addr.sin_port   = htons(udp_server_port);
    memcpy(&udp_server_addr.sin_addr, hostEnd -> h_addr, hostEnd -> h_length);
    
    bool end_state = true; // sentinel to track game progression

    while (1)  // gameplay loop
    {
        if (end_state)  // reset the game if this is the end
        {
            cout << "[GAM] A new secret number is generated" << endl;
            cout << "[GAM] Enter your guess" << endl;
            end_state = false;
        }

        while (get_command(out_pkt) == false) {}  // fetch and assemble packet contents for transmission
        bytes_sent = sendto(udp_server_fd, &out_pkt, sizeof(out_pkt), 0,
                                (sockaddr *) &udp_server_addr, 
                                sizeof(udp_server_addr));  // sending the bytes to server



        get_type_name(out_pkt.type, type_name);  // what type did we send?
        if (!check_bytes(bytes_sent, out_pkt, true, false))  // error handling
        {
            exit(1);
        }

        char server_response[RESPONSE_LEN];  // holds the server's gameplay response

        // receive response
        memset(&inc_pkt, 0, sizeof(inc_pkt));  // resetting packet buffer
        bytes_recv = recv(udp_server_fd, &inc_pkt, sizeof(inc_pkt), 0);  // receiving bytes from server
        get_type_name(inc_pkt.type, type_name);  // what type did we get?
        if (!check_bytes(bytes_recv, inc_pkt, false, false))  // error handling
        {
            exit(1);
        }

        memcpy(&server_response, inc_pkt.buffer, sizeof(server_response));
        char bulls = server_response[A_LOC];  // correct position
        char cows = server_response[B_LOC];  // correct digit

        if (inc_pkt.type == EXIT_GRANT) // exit the game
        {
            exit(1);
        }
        else  // log bulls/cows to console to continue game
        {
            cout << "[GAM] Bulls (A): " << bulls << " | Cows (B): " << cows << endl;
        }

        if (bulls == '4' && cows == '0')  // win state, funnel to restart game state
        {
            cout << "[GAM] You win!" << endl;
            cout << "[GAM] The game is restarting" << endl;
            end_state = true;
        }

    }

    return 0;
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
    string direction = (sending) ? "Sent" : "Recv";  // sending or receiving?
    string verb = (sending) ? "sending" : "receiving";  // action on packet
    if (bytes < 0)  // this transmission was faulty
    {
        cerr << "[ERR] Error " << verb << " message." << endl;
        return false;
    } 
    else  // transmission wasn't faulty
    {
        char type_name[type_name_len];
        get_type_name(packet.type, type_name);
        cout << "[" << pkt_type << "] " << direction << ": " << type_name
                 << " " << packet.buffer << endl;
        return true;
    }
}


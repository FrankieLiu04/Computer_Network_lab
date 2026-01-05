#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <vector>
#include <string.h>
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <unistd.h>
#include <cstdlib>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h> 
#include <sstream>
#include <string>
#include <netdb.h>
#include <algorithm>

#include "message.h"
#include "client.h"

using namespace std;


#define CMD_FILE_EXISTS 9
#define CMD_OVERWRITE 10

int main(int argc, char *argv[])
{
    unsigned short udp_port = 0;
    const char* server_host = "127.0.0.1";

	if ((argc != 3) && (argc != 5))
	{
		cout << "Usage: " << argv[0];
		cout << " [-address <server_host>] -port <udp_port>" << endl;
		return 1;
	}
	else
	{
	
		for (int i = 1; i < argc; i++)
		{				
			if (strcmp(argv[i], "-port") == 0)
				udp_port = (unsigned short) atoi(argv[++i]);
			else if (strcmp(argv[i], "-address") == 0)
			{
				server_host = argv[++i];
				if (argc == 3)
				{
				    cout << "Usage: " << argv[0];
		            cout << " [-address <server_host>] -port <udp_port>" << endl;
		            return 1;
				}
		    }
	        else
	        {
	            cout << "Usage: " << argv[0];
		        cout << " [-address <server_host>] -port <udp_port>" << endl;
		        return 1;
	        }
		}
	}
	

	int client_sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (client_sock < 0) {
		cerr << "Error creating socket" << endl;
		return 1;
	}
	
	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(udp_port);
	

	struct hostent *host_info = gethostbyname(server_host);
	if (host_info == NULL) {
		cerr << "Unknown host: " << server_host << endl;
		close(client_sock);
		return 1;
	}
	memcpy(&server_addr.sin_addr, host_info->h_addr, host_info->h_length);
	

	struct timeval tv;
	tv.tv_sec = 5; 
	tv.tv_usec = 0;
	setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	

    cout << "**************************************************************" << endl;
    cout << "Successfully connect to the server! Please type your command" << endl;
    cout << "**************************************************************" << endl;
	
	Client_State_T client_state = WAITING;
	string in_cmd;
	string filename, new_filename;
	
	
	Cmd_Msg_T cmd_msg;
	Data_Msg_T data_msg;
	Cmd_Msg_T port_msg; 
	
	
    const size_t BUFFER_SIZE = 3000; 
    const int TCP_BASE_PORT = 40000; 
	
	while(true)
	{
	   
	    usleep(100000); 
	    
	    switch(client_state)
	    {
	        case WAITING:
	        {
	            cout<<"$ ";
	            cin>>in_cmd;
	            
	            if(in_cmd == "ls")
	            {
	                client_state = PROCESS_LS;
	            }
	            else if(in_cmd == "send")
	            {
	                client_state = PROCESS_SEND;
	            }
	            else if(in_cmd == "remove")
	            {
	                client_state = PROCESS_REMOVE;
	            }
	            else if(in_cmd == "rename")
                {
                    client_state = PROCESS_RENAME;
                }
	            else if(in_cmd == "shutdown")
	            {
	                client_state = SHUTDOWN;
	            }
	            else if(in_cmd == "quit")
	            {
	                client_state = QUIT;
	            }
	            else
	            {
	                cout<<" - wrong command."<<endl;
	                client_state = WAITING;
	            }
	            break;
	        }
	        case PROCESS_LS:
	        {  
	           
	            cmd_msg.cmd = CMD_LS;
	            cmd_msg.size = 0;
	            memset(cmd_msg.filename, 0, FILE_NAME_LEN);
	            cmd_msg.error = 0;
	            
	            
	            if (sendto(client_sock, &cmd_msg, sizeof(Cmd_Msg_T), 0, 
	                  (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
	                cerr << "Error sending LS command" << endl;
	            } else {
	                
	                socklen_t server_len = sizeof(server_addr);
	                
	                if (recvfrom(client_sock, &data_msg, sizeof(Data_Msg_T), 0,
	                       (struct sockaddr *)&server_addr, &server_len) < 0) {
	                    cerr << "Timeout or error receiving LS response" << endl;
	                } else {
                        if (data_msg.data[0] == '\0') {
                            cout << " - server backup folder is empty." << endl;
                        } else {
                            cout << "Files on server:" << endl;
                            
                            char* file_list = data_msg.data;
                            char* file = file_list;
                            while(*file) {
                                cout << " - " << file << endl;
                                file += strlen(file) + 1;
                            }
                        }
	                }
	            }
		        client_state = WAITING;
	            break;
	        }
	        case PROCESS_SEND:
	        {
	            cin >> filename;
	            
	            ifstream file(filename.c_str(), ios::binary);
	            if (!file) {
	                cout << "File not found: " << filename << endl;
	                client_state = WAITING;
	                break;
	            }
	            
	            file.seekg(0, ios::end);
	            size_t filesize = file.tellg();
	            file.seekg(0, ios::beg);
	            
	            cmd_msg.cmd = CMD_SEND;
	            strncpy(cmd_msg.filename, filename.c_str(), FILE_NAME_LEN-1);
	            cmd_msg.filename[FILE_NAME_LEN-1] = '\0';
	            cmd_msg.size = filesize;
	            cmd_msg.error = 0;
	            
	            cout << " - filesize:" << filesize << endl;
	            
	            if (sendto(client_sock, &cmd_msg, sizeof(Cmd_Msg_T), 0,
	                  (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
	                cerr << "Error sending command" << endl;
	                client_state = WAITING;
	                break;
	            }
	            
	            socklen_t server_len = sizeof(server_addr);
	            Cmd_Msg_T resp_msg;
	            if (recvfrom(client_sock, &resp_msg, sizeof(Cmd_Msg_T), 0,
	                   (struct sockaddr *)&server_addr, &server_len) < 0) {
	                cerr << "Timeout or error receiving response" << endl;
	                client_state = WAITING;
	                break;
	            }
	            
	            cout << " - DEBUG: Received command: " << (int)resp_msg.cmd 
	                 << ", error: " << (int)resp_msg.error 
	                 << ", size: " << resp_msg.size << endl;
	            
	            if (resp_msg.cmd == CMD_FILE_EXISTS) {
	                char answer;
	                cout << " - File " << filename << " already exists on server. Overwrite? (y/n): ";
	                cin >> answer;
	                
	                Cmd_Msg_T confirm_msg;
	                if (tolower(answer) == 'y') {
	                    confirm_msg.cmd = CMD_OVERWRITE;
	                    confirm_msg.error = 0;
	                    confirm_msg.size = filesize; 
	                    strncpy(confirm_msg.filename, filename.c_str(), FILE_NAME_LEN-1); 
	                    confirm_msg.filename[FILE_NAME_LEN-1] = '\0'; 
	                } else {
	                    confirm_msg.cmd = CMD_OVERWRITE;
	                    confirm_msg.error = 1; 
	                }
	                
	                
	                if (sendto(client_sock, &confirm_msg, sizeof(Cmd_Msg_T), 0,
	                      (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
	                    cerr << "Error sending confirmation" << endl;
	                    client_state = WAITING;
	                    break;
	                }
	                
	                if (tolower(answer) != 'y') {
	                    if (recvfrom(client_sock, &resp_msg, sizeof(Cmd_Msg_T), 0,
	                           (struct sockaddr *)&server_addr, &server_len) < 0) {
	                        cerr << "Timeout or error receiving response" << endl;
	                    }
	                    cout << " - File transfer cancelled." << endl;
	                    client_state = WAITING;
	                    break;
	                }
	                
	                if (recvfrom(client_sock, &resp_msg, sizeof(Cmd_Msg_T), 0,
	                       (struct sockaddr *)&server_addr, &server_len) < 0) {
	                    cerr << "Timeout or error receiving server confirmation" << endl;
	                    client_state = WAITING;
	                    break;
	                }
	                
	                if (resp_msg.cmd != CMD_ACK && resp_msg.cmd != CMD_ACK + 10) { 
	                    cerr << "Unexpected server response: " << resp_msg.cmd << endl;
	                    client_state = WAITING;
	                    break;
	                }
	                
	                cout << " - Server ready to receive file" << endl;
	                
	                if (filesize > DATA_BUF_LEN) {
	                    int tcp_port = 0;
	                    
	                    if (resp_msg.cmd == CMD_ACK && resp_msg.size > 0) {
	                        tcp_port = resp_msg.size;
	                    } else {
	                        if (recvfrom(client_sock, &resp_msg, sizeof(Cmd_Msg_T), 0,
	                               (struct sockaddr *)&server_addr, &server_len) < 0) {
	                            cerr << "Timeout or error receiving TCP port" << endl;
	                            client_state = WAITING;
	                            break;
	                        }
	                        tcp_port = resp_msg.size;
	                    }
	                    
	                    cout << " - TCP port:" << tcp_port << endl;
	                    
	                    int tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
	                    if (tcp_sock < 0) {
	                        cerr << "Error creating TCP socket" << endl;
	                        client_state = WAITING;
	                        break;
	                    }
	                    
	                    struct sockaddr_in tcp_server_addr;
	                    memset(&tcp_server_addr, 0, sizeof(tcp_server_addr));
	                    tcp_server_addr.sin_family = AF_INET;
	                    tcp_server_addr.sin_port = htons(tcp_port);
	                    memcpy(&tcp_server_addr.sin_addr, &server_addr.sin_addr, sizeof(struct in_addr));
	                    
	                    if (connect(tcp_sock, (struct sockaddr *)&tcp_server_addr, sizeof(tcp_server_addr)) < 0) {
	                        cerr << "Error connecting to TCP server" << endl;
	                        close(tcp_sock);
	                        client_state = WAITING;
	                        break;
	                    }
	                    
	                    char buffer[BUFFER_SIZE];
	                    size_t bytes_sent = 0;
	                    int total_segments = (filesize + BUFFER_SIZE - 1) / BUFFER_SIZE; 
	                    int segment_count = 0;
	                    
	                    while (bytes_sent < filesize) {
	                        size_t buffer_size = std::min(BUFFER_SIZE, filesize - bytes_sent);
	                        
	                        file.read(buffer, buffer_size);
	                        
	                        ssize_t sent = send(tcp_sock, buffer, buffer_size, 0);
	                        if (sent < 0) {
	                            cerr << "Error sending file data" << endl;
	                            break;
	                        }
	                        
	                        bytes_sent += sent;
	                        segment_count++;
	                        
	                        cout << "Buffer size: " << buffer_size << endl;
	                    }
	                    
	                    char end_marker = 0;
	                    send(tcp_sock, &end_marker, 0, 0);
	                    
	                    cout << "Total Segment Number is: " << segment_count << endl;
	                    
	                    close(tcp_sock);
	                    
	                    if (recvfrom(client_sock, &port_msg, sizeof(Cmd_Msg_T), 0,
	                           (struct sockaddr *)&server_addr, &server_len) < 0) {
	                        cerr << "Timeout or error receiving completion confirmation" << endl;
	                    } else {
	                        if (port_msg.cmd == CMD_ACK && port_msg.error == 0) {
	                            cout << " - file transmission is completed." << endl;
	                        } else {
	                            cerr << "Failed to complete file transfer" << endl;
	                        }
	                    }
	                    
	                    file.close();
	                    client_state = WAITING;
	                    break;
	                }
	            } else if (resp_msg.cmd == CMD_ACK) {
	                
	                if (filesize > DATA_BUF_LEN) {
	                    int tcp_port = resp_msg.size;
	                    cout << " - TCP port:" << tcp_port << endl;
	                    
	                    int tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
	                    if (tcp_sock < 0) {
	                        cerr << "Error creating TCP socket" << endl;
	                        client_state = WAITING;
	                        break;
	                    }
	                    
	                    struct sockaddr_in tcp_server_addr;
	                    memset(&tcp_server_addr, 0, sizeof(tcp_server_addr));
	                    tcp_server_addr.sin_family = AF_INET;
	                    tcp_server_addr.sin_port = htons(tcp_port);
	                    memcpy(&tcp_server_addr.sin_addr, &server_addr.sin_addr, sizeof(struct in_addr));
	                    
	                    if (connect(tcp_sock, (struct sockaddr *)&tcp_server_addr, sizeof(tcp_server_addr)) < 0) {
	                        cerr << "Error connecting to TCP server" << endl;
	                        close(tcp_sock);
	                        client_state = WAITING;
	                        break;
	                    }
	                    
	                    char buffer[BUFFER_SIZE];
	                    size_t bytes_sent = 0;
	                    int total_segments = (filesize + BUFFER_SIZE - 1) / BUFFER_SIZE;  
	                    int segment_count = 0;
	                    
	                    while (bytes_sent < filesize) {
	                        size_t buffer_size = std::min(BUFFER_SIZE, filesize - bytes_sent);
	                        
	                        file.read(buffer, buffer_size);
	                        
	                        ssize_t sent = send(tcp_sock, buffer, buffer_size, 0);
	                        if (sent < 0) {
	                            cerr << "Error sending file data" << endl;
	                            break;
	                        }
	                        
	                        bytes_sent += sent;
	                        segment_count++;
	                        
	                        cout << "Buffer size: " << buffer_size << endl;
	                    }
	                    
	                    char end_marker = 0;
	                    send(tcp_sock, &end_marker, 0, 0);
	                    
	                    cout << "Total Segment Number is: " << segment_count << endl;
	                    
	                    close(tcp_sock);
	                    
	                    if (recvfrom(client_sock, &port_msg, sizeof(Cmd_Msg_T), 0,
	                           (struct sockaddr *)&server_addr, &server_len) < 0) {
	                        cerr << "Timeout or error receiving completion confirmation" << endl;
	                    } else {
	                        if (port_msg.cmd == CMD_ACK && port_msg.error == 0) {
	                            cout << " - file transmission is completed." << endl;
	                        } else {
	                            cerr << "Failed to complete file transfer" << endl;
	                        }
	                    }
	                    
	                    file.close();
	                    client_state = WAITING;
	                    break;
	                } else {
	                    memset(data_msg.data, 0, DATA_BUF_LEN);
	                    file.read(data_msg.data, filesize);
	                    
	                    if (sendto(client_sock, &data_msg, sizeof(Data_Msg_T), 0,
	                          (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
	                        cerr << "Error sending file data" << endl;
	                    } else {
	                        socklen_t server_len = sizeof(server_addr);
	                        Cmd_Msg_T ack_msg;
	                        if (recvfrom(client_sock, &ack_msg, sizeof(Cmd_Msg_T), 0,
	                               (struct sockaddr *)&server_addr, &server_len) < 0) {
	                            cerr << "Timeout or error receiving response" << endl;
	                        } else {
	                            if (ack_msg.cmd == CMD_ACK && ack_msg.error == 0) {
	                                cout << "File sent successfully" << endl;
	                            } else {
	                                cerr << "Failed to send file" << endl;
	                            }
	                        }
	                    }
	                }
	            }
	            
	            file.close();
	            client_state = WAITING;
	            break;
	        }
	        case PROCESS_REMOVE:
	        {
	            cin >> filename;
	            
	            cmd_msg.cmd = CMD_REMOVE;
	            strncpy(cmd_msg.filename, filename.c_str(), FILE_NAME_LEN-1);
	            cmd_msg.filename[FILE_NAME_LEN-1] = '\0';
	            cmd_msg.size = 0;
	            cmd_msg.error = 0;
	            
	            if (sendto(client_sock, &cmd_msg, sizeof(Cmd_Msg_T), 0,
	                  (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
	                cerr << "Error sending delete command" << endl;
	            } else {
	                socklen_t server_len = sizeof(server_addr);
	                Cmd_Msg_T ack_msg;
	                if (recvfrom(client_sock, &ack_msg, sizeof(Cmd_Msg_T), 0,
	                       (struct sockaddr *)&server_addr, &server_len) < 0) {
	                    cerr << "Timeout or error receiving response" << endl;
	                } else {
	                    if (ack_msg.cmd == CMD_ACK) {
	                        if (ack_msg.error == 0) {
	                            cout << " - file is removed." << endl;
	                        } else if (ack_msg.error == 1) {
	                            cout << " - file does not exist on server." << endl;
	                        } else {
	                            cerr << "Failed to delete file" << endl;
	                        }
	                    }
	                }
	            }
	            
	            client_state = WAITING;
	            break;
	        }
	        case PROCESS_RENAME:
	        {
	            cin >> filename >> new_filename;
	            
	            cmd_msg.cmd = CMD_RENAME;
	            strncpy(cmd_msg.filename, filename.c_str(), FILE_NAME_LEN-1);
	            cmd_msg.filename[FILE_NAME_LEN-1] = '\0';
	            cmd_msg.size = 0;
	            cmd_msg.error = 0;
	            
	            memset(data_msg.data, 0, DATA_BUF_LEN);
	            strncpy(data_msg.data, new_filename.c_str(), DATA_BUF_LEN-1);
	            
	            if (sendto(client_sock, &cmd_msg, sizeof(Cmd_Msg_T), 0,
	                  (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
	                cerr << "Error sending rename command" << endl;
	            } else {
	                if (sendto(client_sock, &data_msg, sizeof(Data_Msg_T), 0,
	                      (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
	                    cerr << "Error sending new filename" << endl;
	                } else {
	                    socklen_t server_len = sizeof(server_addr);
	                    Cmd_Msg_T ack_msg;
	                    bool rename_success = false; 
	                    
	                    if (recvfrom(client_sock, &ack_msg, sizeof(Cmd_Msg_T), 0,
	                           (struct sockaddr *)&server_addr, &server_len) < 0) {
	                        cerr << "Timeout or error receiving response" << endl;
	                    } else {
	                        if (ack_msg.cmd == CMD_ACK && ack_msg.error == 1) {
	                            cout << " - source file does not exist on server." << endl;
	                            client_state = WAITING;
	                            break;
	                        } else if (ack_msg.cmd == CMD_ACK && ack_msg.error == 0) {
	                            rename_success = true;
	                        }
	                        
	                        if (ack_msg.cmd == CMD_FILE_EXISTS) {
	                            char answer;
	                            cout << " - File " << new_filename << " already exists on server. Overwrite? (y/n): ";
	                            cin >> answer;
	                            
	                            Cmd_Msg_T confirm_msg;
	                            if (tolower(answer) == 'y') {
	                                confirm_msg.cmd = CMD_OVERWRITE;
	                                confirm_msg.error = 0; 
	                            } else {
	                                confirm_msg.cmd = CMD_OVERWRITE;
	                                confirm_msg.error = 1; 
	                            }
	                            
	                            if (sendto(client_sock, &confirm_msg, sizeof(Cmd_Msg_T), 0,
	                                  (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
	                                cerr << "Error sending confirmation" << endl;
	                                client_state = WAITING;
	                                break;
	                            }
	                            
	                            if (recvfrom(client_sock, &ack_msg, sizeof(Cmd_Msg_T), 0,
	                                   (struct sockaddr *)&server_addr, &server_len) < 0) {
	                                cerr << "Timeout or error receiving response" << endl;
	                                client_state = WAITING;
	                                break;
	                            }
	                            
	                            if (ack_msg.error == 2) {
	                                cout << " - Rename cancelled." << endl;
	                                client_state = WAITING;
	                                break;
	                            }
	                        }
	                        
	                        if (!rename_success && ack_msg.cmd != CMD_ACK) {
	                            struct timeval old_tv;
	                            socklen_t tv_len = sizeof(old_tv);
	                            getsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, &old_tv, &tv_len);
	                            
	                            struct timeval short_tv;
	                            short_tv.tv_sec = 1;
	                            short_tv.tv_usec = 0;
	                            setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, &short_tv, sizeof(short_tv));
	                            
	                            if (recvfrom(client_sock, &ack_msg, sizeof(Cmd_Msg_T), 0,
	                                   (struct sockaddr *)&server_addr, &server_len) >= 0) {
	                                if (ack_msg.cmd == CMD_ACK && ack_msg.error == 0) {
	                                    rename_success = true;
	                                }
	                            }
	                            
	                            setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, &old_tv, sizeof(old_tv));
	                        }
	                    }
	                    
	                    if (rename_success || ack_msg.cmd == CMD_ACK) {
	                        cout << " -file has been renamed." << endl;
	                    }
	                }
	            }
	            
	            client_state = WAITING;
	            break;
	        }	
	        case SHUTDOWN:
	        {
	            cmd_msg.cmd = CMD_SHUTDOWN;
	            cmd_msg.size = 0;
	            cmd_msg.error = 0;
	            
	            if (sendto(client_sock, &cmd_msg, sizeof(Cmd_Msg_T), 0,
	                  (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
	                cerr << "Error sending shutdown command" << endl;
	            } else {
	                cout << " - server is shutdown." << endl;
	            }
	            
	            client_state = WAITING;
	            break;	            
	        }
	        case QUIT:
	        {
	            cout << "Exiting client" << endl;
	            close(client_sock);
	            return 0;	         
	        }
	        default:
	        {
	        	client_state = WAITING;
	            break;
	        }    
	    }
	}
    return 0;
}

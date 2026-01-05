#include <sys/types.h>
#include <sys/stat.h>
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
#include <fcntl.h>

#include "message.h"
#include "server.h"

using namespace std;


#define CMD_FILE_EXISTS 9
#define CMD_OVERWRITE 10


Server_State_T server_state;
string cmd_string[] = {" ", "CMD_LS", "CMD_SEND","CMD_GET","CMD_REMOVE","CMD_RENAME","CMD_SHUTDOWN", "CMD_ACK", "CMD_ACK_PORT", "CMD_FILE_EXISTS", "CMD_OVERWRITE"};

int main(int argc, char *argv[])
{
    unsigned short udp_port = 0;
	if ((argc != 1) && (argc != 3))
	{
		cout << "Usage: " << argv[0];
		cout << " [-port <udp_port>]" << endl;
		return 1;
	}
	else
	{
		
		for (int i = 1; i < argc; i++)
		{				
			if (strcmp(argv[i], "-port") == 0)
				udp_port = (unsigned short) atoi(argv[++i]);
		    else
		    {
		        cout << "Usage: " << argv[0];
		        cout << " [-port <udp_port>]" << endl;
		        return 1;
		    }
		}
	}
	

	int server_sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (server_sock < 0) {
		cerr << "Error creating socket" << endl;
		return 1;
	}
	

	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(udp_port);
	

	if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
		cerr << "Error binding socket to port " << udp_port << endl;
		close(server_sock);
		return 1;
	}
	

	cout << "**************************************************************" << endl;
	cout << "This is the server of Lab2 Remote Backup System" << endl;
	cout << "**************************************************************" << endl;
	cout << "Waiting UDP command @ port number: " << udp_port << endl;
	

	int flags = fcntl(server_sock, F_GETFL, 0);
	fcntl(server_sock, F_SETFL, flags | O_NONBLOCK);
	
	
	checkDirectory("backup");
	

	server_state = WAITING;
	

	Cmd_Msg_T cmd_msg, ack_msg;
	Data_Msg_T data_msg;
	struct sockaddr_in client_addr;
	socklen_t client_len = sizeof(client_addr);
	

	bool first_run = true;
	
    
    const size_t BUFFER_SIZE = 3000; 
    const int TCP_BASE_PORT = 40000; 
    int current_tcp_port = TCP_BASE_PORT;
	
    while(true)
    {
        usleep(100);
        
        
        int recv_len = recvfrom(server_sock, &cmd_msg, sizeof(Cmd_Msg_T), 0,
                          (struct sockaddr *)&client_addr, &client_len);
        
        if (recv_len > 0) {
         
            cout << "[CMD RECEIVED]: " << cmd_string[cmd_msg.cmd] << endl;
            
            switch(cmd_msg.cmd) {
                case CMD_LS:
                    server_state = PROCESS_LS;
                    cout << "Received LS command" << endl;
                    break;
                case CMD_SEND:
                    server_state = PROCESS_SEND;
                    cout << "Received SEND command" << endl;
                    break;
                case CMD_REMOVE:
                    server_state = PROCESS_REMOVE;
                    cout << "Received REMOVE command" << endl;
                    break;
                case CMD_RENAME:
                    server_state = PROCESS_RENAME;
                    cout << "Received RENAME command" << endl;
                    break;
                case CMD_SHUTDOWN:
                    server_state = SHUTDOWN;
                    cout << "Received SHUTDOWN command" << endl;
                    break;
                case CMD_OVERWRITE:
                  
                    cout << "Received OVERWRITE command" << endl;
                    
                
                    cout << " - DEBUG: Filename in OVERWRITE message: '" << cmd_msg.filename << "'" << endl;
                    
                
                    if (cmd_msg.error == 0) {
                      
                        cout << " - client chose to overwrite file." << endl;
                        
                      
                        if (strlen(cmd_msg.filename) == 0) {
                            cout << " - ERROR: Empty filename in OVERWRITE command" << endl;
                            server_state = WAITING;
                            break;
                        }
                        
                        
                        if (cmd_msg.size > DATA_BUF_LEN) {
                            
                            int tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
                            if (tcp_sock < 0) {
                                cerr << "Error creating TCP socket" << endl;
                                break;
                            }
                            
                      
                            int opt = 1;
                            setsockopt(tcp_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
                            
                           
                            struct sockaddr_in tcp_server_addr;
                            memset(&tcp_server_addr, 0, sizeof(tcp_server_addr));
                            tcp_server_addr.sin_family = AF_INET;
                            tcp_server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
                            tcp_server_addr.sin_port = htons(current_tcp_port);
                            
                            if (bind(tcp_sock, (struct sockaddr *)&tcp_server_addr, sizeof(tcp_server_addr)) < 0) {
                                cerr << "Error binding TCP socket" << endl;
                                close(tcp_sock);
                                break;
                            }
                            
                           
                            if (listen(tcp_sock, 5) < 0) {
                                cerr << "Error listening on TCP socket" << endl;
                                close(tcp_sock);
                                break;
                            }
                            
                        
                            ack_msg.cmd = CMD_ACK;
                            ack_msg.error = 0;
                            ack_msg.size = current_tcp_port;
                            
                            cout << " - listen @: " << current_tcp_port << endl;
                            
                            if (sendto(server_sock, &ack_msg, sizeof(Cmd_Msg_T), 0,
                                  (struct sockaddr *)&client_addr, client_len) < 0) {
                                cerr << "Error sending TCP port" << endl;
                                close(tcp_sock);
                                break;
                            }
                            
                    
                            string filename = cmd_msg.filename;
                            string filepath = "backup/" + filename;
                            size_t filesize = cmd_msg.size;
                            
                            cout << " - DEBUG: Will save file as: '" << filepath << "'" << endl;
                            
                     
                            struct sockaddr_in client_tcp_addr;
                            socklen_t client_tcp_len = sizeof(client_tcp_addr);
                            int client_tcp_sock = accept(tcp_sock, (struct sockaddr *)&client_tcp_addr, &client_tcp_len);
                            
                            if (client_tcp_sock < 0) {
                                cerr << "Error accepting TCP connection" << endl;
                                close(tcp_sock);
                                break;
                            }
                            
                            cout << " - connected with client." << endl;
                            
                           
                            ofstream file(filepath.c_str(), ios::binary);
                            if (!file) {
                                cerr << "Failed to create file " << filepath << endl;
                                close(client_tcp_sock);
                                close(tcp_sock);
                                break;
                            }
                            
                           
                            char buffer[BUFFER_SIZE];
                            size_t total_bytes = 0;
                            
                            while (total_bytes < filesize) {
                        
                                ssize_t bytes_received = recv(client_tcp_sock, buffer, BUFFER_SIZE, 0);
                                
                                if (bytes_received <= 0) {
                               
                                    break;
                                }
                                
                           
                                file.write(buffer, bytes_received);
                                total_bytes += bytes_received;
                                
                                cout << bytes_received << endl;
                                cout << " - total bytes received: " << total_bytes << endl;
                            }
                            
                         
                            file.close();
                            close(client_tcp_sock);
                            close(tcp_sock);
                            
                            cout << " - " << filename << " has been received." << endl;
                            current_tcp_port++;  
                           
                            ack_msg.cmd = CMD_ACK;
                            ack_msg.error = 0;
                            cout << " - send acknowledgemet." << endl;
                            
                            sendto(server_sock, &ack_msg, sizeof(Cmd_Msg_T), 0,
                                 (struct sockaddr *)&client_addr, client_len);
                        } else {
                           
                            cout << " - DEBUG: Will receive small file: '" << cmd_msg.filename << "'" << endl;
                            
                        
                            ack_msg.cmd = CMD_ACK;
                            ack_msg.error = 0;
                            sendto(server_sock, &ack_msg, sizeof(Cmd_Msg_T), 0,
                                 (struct sockaddr *)&client_addr, client_len);
                        }
                    } else {
                       
                        cout << " - client chose not to overwrite file." << endl;
                     
                        ack_msg.cmd = CMD_ACK;
                        ack_msg.error = 2; 
                        sendto(server_sock, &ack_msg, sizeof(Cmd_Msg_T), 0,
                             (struct sockaddr *)&client_addr, client_len);
                    }
                    break;
                default:
              
                    server_state = WAITING;
                    cout << "Received unknown command" << endl;
                    break;
            }
        }
        
        switch(server_state)
        {
            case WAITING:
            {
               
                if (first_run) {
                    first_run = false;
                }
                break;
            }
            case PROCESS_LS:
            {
                cout << "Processing LS command" << endl;
                
        
                vector<string> files;
                getDirectory("backup", files);
                
                if (files.empty()) {
                    cout << " - server backup folder is empty." << endl;
                } else {
                    for (size_t i = 0; i < files.size(); i++) {
                        cout << " - " << files[i] << endl;
                    }
                }
                
          
                memset(&data_msg, 0, sizeof(Data_Msg_T));
            
                int offset = 0;
                for (size_t i = 0; i < files.size(); i++) {
                    if (offset + files[i].length() + 1 < DATA_BUF_LEN) {
                        strcpy(data_msg.data + offset, files[i].c_str());
                        offset += files[i].length() + 1; 
                    }
                }
                
               
                sendto(server_sock, &data_msg, sizeof(Data_Msg_T), 0,
                      (struct sockaddr *)&client_addr, client_len);
        
                cout << "**************************************************************" << endl;
                cout << "This is the server of Lab2 Remote Backup System" << endl;
                cout << "**************************************************************" << endl;
                cout << "Waiting UDP command @ port number: " << udp_port << endl;
                
                server_state = WAITING;
                break;
            }
            case PROCESS_SEND:
            {
                cout << "Processing SEND command" << endl;
                cout << " - filename: " << cmd_msg.filename << endl;
                cout << " - filesize: " << cmd_msg.size << endl;
                
                string filename = cmd_msg.filename;
                size_t filesize = cmd_msg.size;
                
           
                string filepath = "backup/" + filename;
                
           
                bool file_exists = false;
                ifstream check_file(filepath.c_str());
                file_exists = check_file.good();
                check_file.close();
                
             
                cout << " - DEBUG: File exists check: " << (file_exists ? "true" : "false") << endl;
                
                if (file_exists) {
                   
                    ack_msg.cmd = CMD_FILE_EXISTS;
                    ack_msg.error = 0;
                    ack_msg.size = filesize;
                    
                    cout << " - DEBUG: Sending cmd: " << (int)ack_msg.cmd 
                         << ", error: " << (int)ack_msg.error 
                         << ", size: " << ack_msg.size << endl;
                    
                
                    if (sendto(server_sock, &ack_msg, sizeof(Cmd_Msg_T), 0,
                             (struct sockaddr *)&client_addr, client_len) < 0) {
                        cerr << "Error sending file exists message" << endl;
                    }
                    
                    
                    server_state = WAITING;
                    break;
                }
                
            
                if (filesize <= DATA_BUF_LEN) {
                    
                    if (recvfrom(server_sock, &data_msg, sizeof(Data_Msg_T), 0,
                           (struct sockaddr *)&client_addr, &client_len) < 0) {
                        cerr << "Error receiving file data" << endl;
                        
                     
                        ack_msg.cmd = CMD_ACK;
                        ack_msg.error = 1;
                    } else {
                      
                        ofstream file(filepath.c_str(), ios::binary);
                        if (file) {
                            file.write(data_msg.data, filesize);
                            file.close();
                            
                        
                            ack_msg.cmd = CMD_ACK;
                            ack_msg.error = 0;
                            
                            cout << "File " << filename << " received and saved" << endl;
                        } else {
                       
                            ack_msg.cmd = CMD_ACK;
                            ack_msg.error = 1;
                            
                            cout << "Failed to save file " << filename << endl;
                        }
                    }
                    
                    sendto(server_sock, &ack_msg, sizeof(Cmd_Msg_T), 0,
                         (struct sockaddr *)&client_addr, client_len);
                } else {
                
                    int tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
                    if (tcp_sock < 0) {
                        cerr << "Error creating TCP socket" << endl;
                        server_state = WAITING;
                        break;
                    }
                    
                 
                    int opt = 1;
                    setsockopt(tcp_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
                    
              
                    struct sockaddr_in tcp_server_addr;
                    memset(&tcp_server_addr, 0, sizeof(tcp_server_addr));
                    tcp_server_addr.sin_family = AF_INET;
                    tcp_server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
                    tcp_server_addr.sin_port = htons(current_tcp_port);
                    
                    if (bind(tcp_sock, (struct sockaddr *)&tcp_server_addr, sizeof(tcp_server_addr)) < 0) {
                        cerr << "Error binding TCP socket" << endl;
                        close(tcp_sock);
                        server_state = WAITING;
                        break;
                    }
                    
                 
                    if (listen(tcp_sock, 5) < 0) {
                        cerr << "Error listening on TCP socket" << endl;
                        close(tcp_sock);
                        server_state = WAITING;
                        break;
                    }
                    
                 
                    ack_msg.cmd = CMD_ACK;  
                    ack_msg.error = 0;
                    ack_msg.size = current_tcp_port;
                    
                    cout << " - DEBUG: Sending CMD_ACK with port: " << current_tcp_port 
                         << ", cmd value: " << (int)ack_msg.cmd << endl;
                    cout << " - listen @: " << current_tcp_port << endl;
                    
                    if (sendto(server_sock, &ack_msg, sizeof(Cmd_Msg_T), 0,
                          (struct sockaddr *)&client_addr, client_len) < 0) {
                        cerr << "Error sending TCP port" << endl;
                        close(tcp_sock);
                        server_state = WAITING;
                        break;
                    }
                    
                  
                    struct sockaddr_in client_tcp_addr;
                    socklen_t client_tcp_len = sizeof(client_tcp_addr);
                    int client_tcp_sock = accept(tcp_sock, (struct sockaddr *)&client_tcp_addr, &client_tcp_len);
                    
                    if (client_tcp_sock < 0) {
                        cerr << "Error accepting TCP connection" << endl;
                        close(tcp_sock);
                        server_state = WAITING;
                        break;
                    }
                    
                    cout << " - connected with client." << endl;
             
                    ofstream file(filepath.c_str(), ios::binary);
                    if (!file) {
                        cerr << "Failed to create file " << filepath << endl;
                        close(client_tcp_sock);
                        close(tcp_sock);
                        server_state = WAITING;
                        break;
                    }
             
                    char buffer[BUFFER_SIZE];
                    size_t total_bytes = 0;
                    
                    while (total_bytes < filesize) {
                     
                        ssize_t bytes_received = recv(client_tcp_sock, buffer, BUFFER_SIZE, 0);
                        
                        if (bytes_received <= 0) {
                            
                            break;
                        }
                        
                  
                        file.write(buffer, bytes_received);
                        total_bytes += bytes_received;
                        
                        cout << bytes_received << endl;
                        cout << " - total bytes received: " << total_bytes << endl;
                    }
                    
                   
                    file.close();
                    close(client_tcp_sock);
                    close(tcp_sock);
                    
                    cout << " - " << filename << " has been received." << endl;
                    current_tcp_port++;  
                    
             
                    ack_msg.cmd = CMD_ACK;
                    ack_msg.error = 0;
                    cout << " - send acknowledgemet." << endl;
                    
                    sendto(server_sock, &ack_msg, sizeof(Cmd_Msg_T), 0,
                         (struct sockaddr *)&client_addr, client_len);
                }
                
                server_state = WAITING;
                break;
            }
            case PROCESS_REMOVE:
            {
                cout << "Processing REMOVE command" << endl;
                
        
                string filename = cmd_msg.filename;
                string filepath = "backup/" + filename;
                
              
                if (!checkFile(filepath.c_str())) {
                   
                    ack_msg.cmd = CMD_ACK;
                    ack_msg.error = 1;
                    
                    cout << " - file " << filename << " does not exist." << endl;
                    sendto(server_sock, &ack_msg, sizeof(Cmd_Msg_T), 0,
                         (struct sockaddr *)&client_addr, client_len);
                    
                   
                    cout << "**************************************************************" << endl;
                    cout << "This is the server of Lab2 Remote Backup System" << endl;
                    cout << "**************************************************************" << endl;
                    cout << "Waiting UDP command @ port number: " << udp_port << endl;
                    
                    server_state = WAITING;
                    break;
                }
                
             
                if (remove(filepath.c_str()) == 0) {
                   
                    ack_msg.cmd = CMD_ACK;
                    ack_msg.error = 0;
                    
                    cout << " - ./backup/" << filename << " has been removed." << endl;
                    cout << " - send acknowledgemet." << endl;
                } else {
                  
                    ack_msg.cmd = CMD_ACK;
                    ack_msg.error = 1;
                    
                    cout << "Failed to delete file " << filename << endl;
                }
                
                sendto(server_sock, &ack_msg, sizeof(Cmd_Msg_T), 0,
                     (struct sockaddr *)&client_addr, client_len);
                
              
                cout << "**************************************************************" << endl;
                cout << "This is the server of Lab2 Remote Backup System" << endl;
                cout << "**************************************************************" << endl;
                cout << "Waiting UDP command @ port number: " << udp_port << endl;
                
		        server_state = WAITING;
                break;
            }
            case PROCESS_RENAME:
            {
                cout << "Processing RENAME command" << endl;
                
              
                string old_filename = cmd_msg.filename;
                string old_filepath = "backup/" + old_filename;
                
               
                if (!checkFile(old_filepath.c_str())) {
                
                    ack_msg.cmd = CMD_ACK;
                    ack_msg.error = 1;
                    
                    cout << " - file " << old_filename << " does not exist." << endl;
                    sendto(server_sock, &ack_msg, sizeof(Cmd_Msg_T), 0,
                         (struct sockaddr *)&client_addr, client_len);
                    
                  
                    cout << "**************************************************************" << endl;
                    cout << "This is the server of Lab2 Remote Backup System" << endl;
                    cout << "**************************************************************" << endl;
                    cout << "Waiting UDP command @ port number: " << udp_port << endl;
                    
                    server_state = WAITING;
                    break;
                }
                
            
                if (recvfrom(server_sock, &data_msg, sizeof(Data_Msg_T), 0,
                       (struct sockaddr *)&client_addr, &client_len) < 0) {
                    cerr << "Error receiving new filename" << endl;
                } else {
                 
                    string new_filename = data_msg.data;
                    string new_filepath = "backup/" + new_filename;
                    
                 
                    if (checkFile(new_filepath.c_str())) {
                   
                        ack_msg.cmd = CMD_FILE_EXISTS;
                        ack_msg.error = 0;
                        
                        cout << " - target file " << new_filename << " already exists. Asking for confirmation." << endl;
                        sendto(server_sock, &ack_msg, sizeof(Cmd_Msg_T), 0,
                             (struct sockaddr *)&client_addr, client_len);
                        
                     
                        Cmd_Msg_T confirm_msg;
                        if (recvfrom(server_sock, &confirm_msg, sizeof(Cmd_Msg_T), 0,
                               (struct sockaddr *)&client_addr, &client_len) < 0) {
                            cout << " - no confirmation received. Aborting rename." << endl;
                            server_state = WAITING;
                            break;
                        }
                        
                        if (confirm_msg.cmd != CMD_OVERWRITE || confirm_msg.error != 0) {
                            cout << " - client chose not to overwrite target file." << endl;
                        
                            ack_msg.cmd = CMD_ACK;
                            ack_msg.error = 2; 
                            sendto(server_sock, &ack_msg, sizeof(Cmd_Msg_T), 0,
                                 (struct sockaddr *)&client_addr, client_len);
                            
                            server_state = WAITING;
                            break;
                        }
                        
                  
                        remove(new_filepath.c_str());
                        cout << " - client chose to overwrite target file." << endl;
                    }
                    
                  
                    if (rename(old_filepath.c_str(), new_filepath.c_str()) == 0) {
                       
                        ack_msg.cmd = CMD_ACK;
                        ack_msg.error = 0;
                        
                        cout << " -the file has been renamed to " << new_filename << "." << endl;
                        cout << " -send acknowledgement" << endl;
                    } else {
                       
                        ack_msg.cmd = CMD_ACK;
                        ack_msg.error = 1;
                        
                        cout << "Failed to rename file from " << old_filename << " to " << new_filename << endl;
                    }
                    
                    sendto(server_sock, &ack_msg, sizeof(Cmd_Msg_T), 0,
                         (struct sockaddr *)&client_addr, client_len);
                }
                
                
                cout << "**************************************************************" << endl;
                cout << "This is the server of Lab2 Remote Backup System" << endl;
                cout << "**************************************************************" << endl;
                cout << "Waiting UDP command @ port number: " << udp_port << endl;
                
                server_state = WAITING;
                break;
            }
            case SHUTDOWN:
            {
                cout << "Processing SHUTDOWN command" << endl;
                cout << " - send acknowledgemet." << endl;
                close(server_sock);
                return 0;
            }
            default:
            {
           		server_state = WAITING;
                break;
            }
        }
    }
    return 0;
}


int checkDirectory (string dir)
{
	DIR *dp;
	if((dp  = opendir(dir.c_str())) == NULL) {
     
        if(mkdir(dir.c_str(), S_IRWXU) == 0)
            cout<< " - Note: Folder "<<dir<<" does not exist. Created."<<endl;
        else
            cout<< " - Note: Folder "<<dir<<" does not exist. Cannot created."<<endl;
        return errno;
    }
    closedir(dp);
    return 0;  
}


int getDirectory (string dir, vector<string> &files)
{
    DIR *dp;
    struct dirent *dirp;
    if((dp  = opendir(dir.c_str())) == NULL) {
       
        if(mkdir(dir.c_str(), S_IRWXU) == 0)
            cout<< " - Note: Folder "<<dir<<" does not exist. Created."<<endl;
        else
            cout<< " - Note: Folder "<<dir<<" does not exist. Cannot created."<<endl;
        return errno;
    }

    int j=0;
    while ((dirp = readdir(dp)) != NULL) {
    	
        if((string(dirp->d_name)!=".") && (string(dirp->d_name)!=".."))
        	files.push_back(string(dirp->d_name));
    }
    closedir(dp);
    return 0;
}

bool checkFile(const char *fileName)
{
    ifstream infile(fileName);
    return infile.good();
}


#include "server.hpp"

#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <regex>

#define EPOLL_QUEUE_LEN 10000
#define MAXEVENTS 64

namespace LabsTest {

const std::string Server::null_val("null");

Server::Server(const std::string& listen_address, int listen_port)
    : listen_fd(-1) , db(10000)
{
    std::cout << "creating server" << std::endl;

    sockaddr_in listen_sockaddr_in;
    std::memset(&listen_sockaddr_in, 0, sizeof(listen_sockaddr_in));
    listen_sockaddr_in.sin_family = AF_INET;
    inet_aton(listen_address.c_str(), &listen_sockaddr_in.sin_addr);
    listen_sockaddr_in.sin_port = htons(listen_port);

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(listen_fd < 0) {
        throw_error("could not create socket", errno);
    }

    int t = 1;
    if(setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &t, sizeof(t))) {
        throw_error("could not set SO_REUSEADDR", errno);
    }

    if(bind(listen_fd, (struct sockaddr*) &listen_sockaddr_in, sizeof(listen_sockaddr_in))) {
        throw_error("could not bind listen socket", errno);
    }

    if(listen(listen_fd, 48)) {
        throw_error("could not listen on socket", errno);
    }

    //picked up by test_server.py to know server is ready
    //this line must be output after listen returns successfully 
    std::cout << "listening on " << listen_address << ":" << listen_port << std::endl;
}

int Server::accept_new_connection() {
    sockaddr_in peer_addr;
    socklen_t peer_addr_size = sizeof(peer_addr);
    std::memset(&peer_addr, 0, peer_addr_size);
    
    //peer_fd is the file descriptor for the socket of the newly connected client
    int peer_fd = accept4(listen_fd, (struct sockaddr*) &peer_addr, &peer_addr_size, SOCK_CLOEXEC);
  
    if (peer_fd < 0) {
        throw_error("error accepting connection", errno);
    }

    std::cout << "accepted connection, peer_fd=" << peer_fd << std::endl;

    return peer_fd;
}

void Server::make_new_connection(int epfd, epoll_event &event) {
	int fd = accept_new_connection();
	tarpit_buffers.push_back(new std::string());
	tarpit_buffer_map.insert(std::make_pair(fd, tarpit_buffers[tarpit_buffers.size() - 1]));
	event.data.fd = fd;
	event.events = EPOLLIN | EPOLLHUP;
	if((epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &event)) == -1){
		throw std::runtime_error("epoll failed to add socket");
	}
	std::cout << "added connection to epoll with descriptor " << fd << std::endl;
}

//TODO: this really should be mutliple functions... refactor asap
void Server::run() {
    std::cout << "running ..." << std::endl;
    int epfd;
    if((epfd = epoll_create(EPOLL_QUEUE_LEN)) == -1) {
        throw std::runtime_error("epoll could not be created");
    }

    static struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLPRI | EPOLLERR | EPOLLHUP | EPOLLRDHUP;
    ev.data.fd = listen_fd;
    if((epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev)) == -1){
        throw std::runtime_error("epoll failed to add socket");
    }

    int num_events;
    struct epoll_event event, curr_event;
    struct epoll_event *events = static_cast<epoll_event*>(calloc(MAXEVENTS, sizeof curr_event));

    char buffer[1024];
    const size_t buffer_size = sizeof(buffer);
    while (true) {
        num_events = epoll_wait(epfd, events, MAXEVENTS, 30000);

        for(int i = 0; i < num_events; ++i) {
            std::cout << "recieved epoll event" << num_events << std::endl;
            curr_event = events[i];

            if((curr_event.events & EPOLLERR)  ||
               (curr_event.events & EPOLLHUP)  ||
               (!(curr_event.events & EPOLLIN))) {
                throw std::runtime_error("epoll errored on event");
                close(curr_event.data.fd);
            }
            else if(curr_event.events & EPOLLRDHUP) {
                std::cout << "closed descriptor via EPOLLRDHUP " << curr_event.data.fd << std::endl;
                close(curr_event.data.fd);
            }
            // make a new connection
            else if(curr_event.data.fd == listen_fd) {
            	make_new_connection(epfd, event);
	    }
            else {
                // Read data and do stuff
                char * token_string;
                ssize_t num_bytes = 0;
                memset(buffer, 0, sizeof(buffer));
                std::string *full_buffer = tarpit_buffer_map[curr_event.data.fd];
                std::cout << "reading from descriptor " << curr_event.data.fd << std::endl;
                if((num_bytes = read(curr_event.data.fd, buffer, buffer_size - 1)) == -1) {
                    throw_error("read error", errno);
                }
                std::cout << "read " << num_bytes << " bytes from descriptor " << curr_event.data.fd << std::endl;

                *full_buffer += std::string(buffer);
                std::cout << "size: " << full_buffer->size() << std::endl;

                if(buffer[num_bytes - 1] != '\n') {
                    break;
                }

                // strtok is just a bit faster than regex
                // this got weird because I'm using strings to store buffers and a bit last minute
                // TODO use std::strings instead of hacking around using old strtok
                //  -- it was good when I didn't want to copy into a string buffer, but that turned out to be necessary
                std::vector<char> tokbuf(full_buffer->size() + 1);
                strcpy(&tokbuf[0], full_buffer->c_str());
                token_string = strtok(&tokbuf[0], " \n\t");
                std::string end_response = "";
                do {
                    std::cout << token_string << std::endl;
                    char * command = token_string;
                    if(strcmp("set", command) == 0) {
                        token_string = strtok(NULL, " \n\t");
                        char * key = token_string;
                        token_string = strtok(NULL, " \n\t"); 
                        char * value = token_string;
                        if(key == NULL || value == NULL) {
                            // malformed request
                            continue;
                        }
                        else {
                            std::string skey(key);
                            std::string svalue(value);
                            db.set(skey, svalue);
                            std::cout << "set " << skey << "=" << svalue << std::endl;
                            std::string response = skey + '=' + svalue + '\n';
                            end_response += response;
                        }
                    }
                    else if(strcmp("get", command) == 0) {
                        token_string = strtok(NULL, " \n\t");
                        char * key = token_string;
                        if(key == NULL) {
                            continue;
                        }
                        else {
                            std::string skey(key);
                            std::string svalue;
                            svalue = db.get(skey);
                            std::cout << "get " << skey << "=" << svalue << std::endl;
                            std::string response = skey + '=' + svalue + '\n';
                            end_response += response;
                        }
                    }
                    else if(strcmp("quit", command) == 0) {
                        std::cout << "requested quit from descriptor " << curr_event.data.fd << std::endl;
                        close(curr_event.data.fd);
                    }
                    else {
                        // malformed request
                    }
                    token_string = strtok(NULL, " \n\t");
                } while(token_string != NULL);
                while(end_response.size() > 0) {
                    memset(buffer, 0, sizeof(buffer));
                    strncpy(buffer, end_response.c_str(), sizeof(buffer));
                    if((num_bytes = write(curr_event.data.fd, buffer, strlen(buffer))) == -1) {
                        throw_error("write error", errno);
                    }
                    end_response.erase(0, 1024);
                    std::cout << "wrote " << num_bytes << " bytes to descriptor " << curr_event.data.fd << std::endl;
                }
                *full_buffer = "";
            }
        }
    }
}

void Server::throw_error(const char* msg_, int errno_) {
    std::string msg = msg_ + std::string(" errno=") + std::to_string(errno_);
    throw std::runtime_error(msg);
}

void Server::epoll_func() {
}

}


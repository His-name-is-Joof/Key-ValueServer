#ifndef EPOCHLABS_TEST_SERVER_HPP
#define EPOCHLABS_TEST_SERVER_HPP

#include <string>
#include <vector>
#include <sys/epoll.h>
#include "Database.hpp"

namespace LabsTest {

class Server {
public:
    Server(const std::string& listen_address, int listen_port);
    void run();
    static const std::string null_val;
  
private:
    int listen_fd;
    //add your members here
    Database<std::string, std::string, null_val> db;

    int accept_new_connection();
    void throw_error(const char* msg_, int errno_);
    //add your methods here
    void epoll_func();
    void handle_request();
    void make_new_connection(int epfd, epoll_event &event);
    std::vector<std::string*> tarpit_buffers;
    std::unordered_map<int, std::string*> tarpit_buffer_map;
};

}

#endif

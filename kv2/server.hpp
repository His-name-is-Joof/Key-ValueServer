#ifndef EPOCHLABS_TEST_SERVER_HPP
#define EPOCHLABS_TEST_SERVER_HPP

#include <string>
#include "Threadpool.hpp"

namespace EpochLabsTest {

// Main thread will listen for requests to connect and add them to a polling_job_queue
// TODO: Seperate thread for accepting requests
//
// polliing_threads will poll a number of peers to see if a message has been sent and place requests into database_job_queue
// TODO: Dynamically allocate threads as needed up to some maximum
// database_threads 
class Server {
public:
    Server(const std::string& listen_address, int listen_port);
    void run();
  
private:
    int listen_fd;
    Threadpool<void()> polling_threads;
    Threadpool<void()> database_threads;
    BoundedQueue<int> polling_job_queue; // queue contains file descriptors
    BoundedQueue<std::string> database_job_queue;
    Database db;

    int accept_new_connection();
    void throw_error(const char* msg_, int errno_);
    // Pulls jobs from job queue
    void polling_thread_main();
    // 
    void process_job(std::string& peer_addr);

    struct polling_info {
    }
};

}

#endif

#ifndef EPOCHLABS_THREADPOOL_HPP
#define EPOCHLABS_THREADPOOL_HPP

#include <functional>
#include <thread>
#include <vector>

namespace EpochLabsTest {

template <typename FunctionType, typename... Arguments>
class Threadpool {
public:
    Threadpool(int numthreads, std::function<FunctionType> threadfunc, const Arguments&... args) {
        for(auto &n : numthreads) {
            threads.push_back(std::thread(threadfunc(args...)));
        }
    }
    ~Threadpool() {
        for(auto &n : threads) {
            n.join();
        }
    }

private:
    std::vector<std::thread> threads;
};

}

#endif

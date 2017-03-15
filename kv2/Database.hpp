#include <unordered_map>
#include <mutex>

namespace EpochLabsTest {

// Provides a thread safe key-value store using an unordered map
// Locks on single read or write requests
//
// TODO:    Test multiple read / single write.
//              - Batching requests
//              - This may be largely irrelevent because so little computation is going on 
//              and batching may require reordering which may take more time than simply locking and doing a single set/get
//          Remove single point of failure.
//              - If the server fails, we lose everything
//              - This problem is beginning to be equivalent to EpochLab's entire company and multiple areas of research...
template <typename Key, typename Value, Value null_val>
class Database {
public:
    void set(Key k, Value v) {
        std::unique_lock<std::mutex> table_lock(mtx);
        table_lock.lock();
        if(table.count(k)) {
            table.erase(k);
        }
        table.insert(std::make_pair(k, v));
    }
    Value get(Key k) {
        std::unique_lock<std::mutex> table_lock(mtx);
        table_lock.lock();
        Value ret_val;

        // unordered_map[] adds an empty element if null and might force a resize, so we use at()
        // resizes wont matter in a set request (we don't need the value returned), and aren't in a normal get()
        try {
            ret_val = table.at(k);
        }
        catch(const std::out_of_range & oor) {
            ret_val = null_val;
        }
        return ret_val;
    }
private:
    std::unordered_map<T> table;
    std::mutex mtx;
};

}

#include <unordered_map>
#include <mutex>
#include <iostream>

namespace EpochLabsTest {

// Provides a thread safe key-value store using an unordered map
// Locks on single read or write requests
//
// TODO:    Remove single point of failure.
//              - If the server fails, we lose everything
//              - This problem is beginning to be equivalent to EpochLab's entire company and multiple areas of research...
template <typename Key, typename Value, const Value & null_val>
class Database {
public:
    Database(int table_elems) {
        // TODO: if reserve is too large, there is not error message (due to how unordered_map works)
        // this will cause lots of unnecessary and expensive reallocs.
        std::cout << "reserving space in unordered_map for " << table_elems << " elements" << std::endl;
        table.reserve(table_elems);
    }
    void set(Key k, Value v) {
        if(table.count(k)) {
            table.erase(k);
        }
        table.insert(std::make_pair(k, v));
    }
    Value get(Key k) {
        Value ret_val;

        // unordered_map[] adds an empty element if null and might eventually force a resize, so we use at()
        try {
            ret_val = table.at(k);
        }
        catch(const std::out_of_range & oor) {
            ret_val = null_val;
        }
        return ret_val;
    }
private:
    std::unordered_map<Key, Value> table;
};

}

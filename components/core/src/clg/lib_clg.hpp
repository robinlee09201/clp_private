#pragma once
// C++ libraries
#include <iostream>
#include <string>
#include <vector>

// Project headers

namespace clg {
    struct Archive_addresses {
        std::pair<char *, size_t> logtype_dictionary;
        std::pair<char *, size_t> logtype_segment_index;
        std::pair<char *, size_t> metadata;
        std::pair<char *, size_t> metadata_db;
        std::pair<char *, size_t> var_dictionary;
        std::pair<char *, size_t> var_segment_index;
        std::vector<std::pair<char *, size_t>> segments;
    };
    class lib_clg {
    public:
        // Constructors
        lib_clg () = default;
        
        // Methods
        int search(int argc, const char ** argv, struct Archive_addresses& archive_addresses);
    private:
        // Methods
        
        // Variables
        
    };
}
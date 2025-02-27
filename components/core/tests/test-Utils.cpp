// C libaries
#include <sys/stat.h>
#include <unistd.h>

// C++ libraries
#include <boost/foreach.hpp>
#include <boost/range/combine.hpp>
#include <chrono>
#include <ctime>
#include <iostream>
#include <ratio>

// Catch2
#include <Catch2/single_include/catch2/catch.hpp>

// Project headers
#include "../src/Utils.hpp"

using namespace std;

TEST_CASE("create_directory_structure", "[create_directory_structure]") {
    struct stat s = {};
    string path;

    path = "a/b/c";
    REQUIRE(ErrorCode_Success == create_directory_structure(path, 0700));
    REQUIRE(stat(path.c_str(), &s) == 0);

    path = "d/e/f/";
    REQUIRE(ErrorCode_Success == create_directory_structure(path, 0700));
    REQUIRE(stat(path.c_str(), &s) == 0);

    path = "/tmp/5807";
    REQUIRE(ErrorCode_Success == create_directory_structure(path, 0700));
    REQUIRE(stat(path.c_str(), &s) == 0);

    REQUIRE(0 == rmdir("a/b/c"));
    REQUIRE(0 == rmdir("a/b"));
    REQUIRE(0 == rmdir("a"));

    REQUIRE(0 == rmdir("d/e/f"));
    REQUIRE(0 == rmdir("d/e"));
    REQUIRE(0 == rmdir("d"));

    REQUIRE(0 == rmdir("/tmp/5807"));
}

TEST_CASE("get_parent_directory_path", "[get_parent_directory_path]") {
    // Corner cases
    // Anything without a slash should return "."
    REQUIRE(get_parent_directory_path(".") == ".");
    REQUIRE(get_parent_directory_path("..") == ".");
    REQUIRE(get_parent_directory_path("abc") == ".");
    // A single slash, at the beginning, should always return "/"
    REQUIRE(get_parent_directory_path("/") == "/");
    REQUIRE(get_parent_directory_path("/.") == "/");
    REQUIRE(get_parent_directory_path("/..") == "/");
    REQUIRE(get_parent_directory_path("/abc") == "/");

    // Normal cases
    REQUIRE(get_parent_directory_path("//abc/./def//../def/.///") == "/abc");
}

TEST_CASE("get_unambiguous_path", "[get_unambiguous_path]") {
    // Base cases (should not modify anything)
    REQUIRE(get_unambiguous_path("/") == "/");
    REQUIRE(get_unambiguous_path("abc") == "abc");
    REQUIRE(get_unambiguous_path("/abc") == "/abc");
    REQUIRE(get_unambiguous_path("/abc/def") == "/abc/def");

    // Corner cases
    REQUIRE(get_unambiguous_path(".").empty());
    REQUIRE(get_unambiguous_path("..").empty());
    REQUIRE(get_unambiguous_path("////") == "/");
    REQUIRE(get_unambiguous_path("/./.././//../") == "/");
    REQUIRE(get_unambiguous_path("./.././//../").empty());
    REQUIRE(get_unambiguous_path("/abc/def/.././../") == "/");
    REQUIRE(get_unambiguous_path("abc/def/.././../").empty());

    // Normal cases
    REQUIRE(get_unambiguous_path("/abc///def/../ghi/./") == "/abc/ghi");
    REQUIRE(get_unambiguous_path("abc///def/../ghi/./") == "abc/ghi");
    REQUIRE(get_unambiguous_path("../abc///def/../ghi/./") == "abc/ghi");
}

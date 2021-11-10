#include "util/coding.h"
#include <iostream>
#include <string>

int main() {
    // std::string str = "abcccccccccccccccccccccccccccccc";
    char* dst = new char[5];
    unsigned int val = 1234;
    leveldb::EncodeVarint32(dst, val);

}
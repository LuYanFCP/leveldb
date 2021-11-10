#include "leveldb/db.h"

#include <fcntl.h>
#include <iostream>
#include <cstdlib>
#include <unistd.h>

int main()
{
    leveldb::DB *db;
    leveldb::Options config;
    config.create_if_missing = true;
    leveldb::Status status = leveldb::DB::Open(config, "./test.db", &db);
    if (!status.ok()) {
        std::cout << "init error exit!" << std::endl;
    }   
    db->Put(leveldb::WriteOptions(), "key", "value");
    std::string value; 
    status = db->Get(leveldb::ReadOptions(), "key", &value);
    if (status.ok()) std::cout << "Key: => " << value << std::endl;
}
#include "FAST.h"

void DSSE::Setup()
{

    // Step 1: Get the 128-bit random secret key using the DSSE public function Get_Client_sk()
    this->Data.client_sk = Get_Client_sk();

    // Step 2: Initialize three RocksDB database instances (map1, map2)
    rocksdb::Options options;
    options.create_if_missing = true;

    rocksdb::DB* db1;
    rocksdb::Status status1 = rocksdb::DB::Open(options, "Sigma_map1", &db1);
    if (!status1.ok()) 
    {
        std::cerr << "Error opening RocksDB for map1: " << status1.ToString() << std::endl;
        exit(1);
    }

    rocksdb::DB* db2;
    rocksdb::Status status2 = rocksdb::DB::Open(options, "Server_map2", &db2);
    if (!status2.ok()) {
        std::cerr << "Error opening RocksDB for map2: " << status2.ToString() << std::endl;
        exit(1);
    }


    this->Data.map1 = db1;
    this->Data.map2 = db2;

    cout << " FAST: Setup Ends Here;" << endl;

};
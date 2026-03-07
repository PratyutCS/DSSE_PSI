#include <fstream>
#include "FAST.h"

void DSSE::Setup(const string &storage_path)
{
    string path1 = "Sigma_map1";
    string key_path = "master_key.bin";
    
    if (!storage_path.empty()) {
        path1 = storage_path + "/Sigma_map1";
        key_path = storage_path + "/master_key.bin";
    }

    // Step 1: Persist or Load the 128-bit secret key
    ifstream key_file_in(key_path, ios::binary);
    if (key_file_in.is_open()) {
        unsigned char key_buf[16];
        key_file_in.read(reinterpret_cast<char*>(key_buf), 16);
        this->secret_key.Assign(key_buf, 16);
        key_file_in.close();
    } else {
        ofstream key_file_out(key_path, ios::binary);
        if (key_file_out.is_open()) {
            key_file_out.write(reinterpret_cast<const char*>(this->secret_key.data()), 16);
            key_file_out.close();
        }
    }
    
    this->Data.client_sk = Get_Client_sk();

    // Step 2: Initialize RocksDB database instance (map1)
    if (this->Data.map1 != nullptr) {
        delete this->Data.map1;
        this->Data.map1 = nullptr;
    }

    rocksdb::Options options;
    options.create_if_missing = true;

    rocksdb::DB* db1;
    rocksdb::Status status1 = rocksdb::DB::Open(options, path1, &db1);
    if (!status1.ok()) 
    {
        std::cerr << "Error opening RocksDB for map1: " << status1.ToString() << std::endl;
        // Don't exit(1), just let it be null so we don't crash the whole JVM
        this->Data.map1 = nullptr;
    } else {
        this->Data.map1 = db1;
    }

    cout << " FAST: Setup Ends Here;" << endl;
};
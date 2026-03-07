#include "FAST.h"

void DSSE::Update_client(const string &ind, const string &keyword, const bool op, tuple<string, string> &u_token)
{
    // Client Side
    string t_keyword;
    
    encryptAES(this->Data.client_sk, sha256(keyword), t_keyword);                                      // As F a PRF AES256 

    string st_c;
    int c = 0;
    int decision = 0;

    decision = Retrive_tupple_DB(this->Data.map1, keyword, st_c, c);                                   // Retrieveing the value stored (st, c) corresponding to the "keyword" in the database map1 if exists
    if(decision == 0)
    {
        st_c = generate128BitString();
        c = 0;
    }

    SecByteBlock k_c_plus_1 = generateRandom128BitKey();

    string st_c_plus_1;
    encryptAES(k_c_plus_1, st_c, st_c_plus_1);
    
    c = c + 1;

    Store_tupple_DB(this->Data.map1, keyword, st_c_plus_1, c);

    // display_map(map1, f2);                                                                // Looking all the stored key pairs

    string temp1, temp2;
    temp1 = t_keyword + st_c_plus_1;

    string ind_fixed = ind;
    ind_fixed.resize(15, '\0'); // Ensure ind is exactly 15 bytes

    temp2 = ind_fixed + (op == true ? "1" : "0") + SecByteBlockToString(k_c_plus_1);                             // the size of ind here is 15 byte and op takes 1 byte and k_c_plus_1 takes 16 byte so total ind||op||k_c+1 takes 32 byte

    get<0>(u_token) = sha256(temp1);                                                        // u
    get<1>(u_token) = xorStrings(temp2, hashSHAKE(temp1,32));                               // e
    
}

void DSSE::Update_server(const tuple<string, string> &u_token)
{
    // Server Side
    rocksdb::Status status  = this->Data.map2->Put(rocksdb::WriteOptions(), get<0>(u_token), get<1>(u_token));
    if(!status.ok())
    {
        std::cerr << "Error reading from RocksDB:" << status.ToString() << std::endl;
    }   
}
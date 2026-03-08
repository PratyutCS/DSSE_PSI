#ifndef FAST
#define FAST

#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <stdexcept>
#include <random>
#include <iomanip>

#include <cryptopp/osrng.h>
#include <cryptopp/sha.h>
#include <cryptopp/shake.h>
#include <cryptopp/aes.h>
#include <cryptopp/filters.h>
#include <cryptopp/modes.h>
#include <cryptopp/secblock.h>

#include <rocksdb/db.h>
#include <rocksdb/options.h>

using namespace std;
using namespace CryptoPP;

void encryptAES(const SecByteBlock &key, const string &plaintext, string &ciphertext); 
void decryptAES(const SecByteBlock &key, const string &ciphertext, string &plaintext); 

string SecByteBlockToString(const SecByteBlock& block);
SecByteBlock StringToSecByteBlock(const string& str); 

string xorStrings(const std::string& str1, const std::string& str2);
CryptoPP::SecByteBlock generateRandom128BitKey();

string generate128BitString();
string sha256(const std::string &input);
string hashSHAKE(const std::string& input, size_t outputLength);

void Store_tupple_DB(rocksdb::DB *map, const string keyword, const string st, const int c);
int Retrive_tupple_DB(rocksdb::DB *map, const string &keyword, string &st, int &c);

class DSSE 
{
    private:
        SecByteBlock secret_key;

    public:
        struct SetupResult 
        {
            SecByteBlock client_sk;
            rocksdb::DB* map1;
            rocksdb::DB* map2;
        };

        SetupResult Data;

        DSSE()
        {
            cout << "DSSE FAST: Begins "<<endl;
            secret_key = generateRandom128BitKey();
        }

        ~DSSE()  
        {
            if (Data.map1) { delete Data.map1; Data.map1 = nullptr; }
            if (Data.map2) { delete Data.map2; Data.map2 = nullptr; }
            cout << "DSSE FAST: Ends " << endl;
        }

        SecByteBlock Get_Client_sk()
        {
            return secret_key;
        }

        void Set_Client_sk(const SecByteBlock &key) {
            secret_key = key;
        }

        void Setup();
        void Update_client(const string &ind, const string &keyword, const bool op, tuple<string, string> &u_token);     
        void Update_server(const tuple<string, string> &u_token);
        void Search_client(const string &keyword, tuple<string, string, int> &s_token);
        void Search_server(const tuple<string, string, int> &s_token, vector<string> &search_result);
};

#endif
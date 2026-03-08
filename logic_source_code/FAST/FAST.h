#ifndef FAST
#define FAST

#include <iostream>
#include <sstream>

#include <vector>
#include <string>
#include <stdexcept>
#include <random>                                                           // For random number generation
#include <iomanip>                                                          // For std::fixed and std::setprecision


#include <cryptopp/osrng.h>                                                 // For AutoSeededRandomPool
#include <cryptopp/sha.h>
#include <cryptopp/shake.h>
#include <cryptopp/aes.h>
#include <cryptopp/filters.h>
#include <cryptopp/modes.h>
#include <cryptopp/secblock.h>

#include <rocksdb/db.h>
#include <rocksdb/options.h>

//#include <rocksdb/iterator.h>                                               // to iterate the db over the keys



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

void Store_tupple_DB(rocksdb::DB *map, const string keyword, const string st, const int c);                   // It stores (st, c) type tuple in a database instance.
int Retrive_tupple_DB(rocksdb::DB *map, const string &keyword, string &st, int &c);

//void display_map(rocksdb::DB* map);

class DSSE 
{
    private:

        SecByteBlock secret_key;

    public:
        struct SetupResult 
        {
            SecByteBlock client_sk;
            rocksdb::DB* map1;                                                                          // client-side map Sigma
            rocksdb::DB* map2;                                                                          // server-side map T
        };

        SetupResult Data;

        // Constructor function
        DSSE()
        {
            cout << "DSSE FAST: Begins "<<endl;
            secret_key = generateRandom128BitKey();                                                     // Initialising the secret key 

        }
        //Destructor function
        ~DSSE()  
        {
            cout << "DSSE FAST: Ends " << endl;
        }

        SecByteBlock Get_Client_sk()                                                                    // Client can access the secret_key using this function
        {
            return secret_key;
        }

        void Setup();                                                                       // Declaration of function for Setup Algorithm

        void Update_client(const string &ind, const string &keyword, const bool op, tuple<string, string> &u_token);     
        void Update_server(const tuple<string, string> &u_token);
        
        void Search_client(const string &keyword, tuple<string, string, int> &s_token); // Declaration of function for Search protocol
        void Search_server(const tuple<string, string, int> &s_token, vector<string> &search_result);
};

#endif 
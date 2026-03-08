#include "FAST.h"

// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% Utitlites: Functions and their Definitions %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

void Store_tupple_DB(rocksdb::DB *map, const string keyword, const string st, int c)      // Function to store (st, c) in a Map corresponding to keyword
{
    if(!map)
    {
        cerr << "Error: Database is not initialised !" << endl;
    }
    std::ostringstream oss;             // string combiner in c++
    oss << st << c;                     // Combining st and c
    string st_c_tupple = oss.str();

    // Store in RocksDB
    rocksdb::Status status = map->Put(rocksdb::WriteOptions(), keyword, st_c_tupple);
    if (!status.ok()) 
    {
        std::cerr << "Error storing tuple in RocksDB: " << status.ToString() << std::endl;
    }

}

int Retrive_tupple_DB(rocksdb::DB *map, const string &keyword, string &st, int &c)          // Function to retrieve the stored value (st, c) in a Map corresponding to keyword
{
    if(!map)
    {
        cerr << "Error: Database is not initialised !" << endl;
    }

    // Retriving the tupple
    string value;
    rocksdb::Status status  = map->Get(rocksdb::ReadOptions(), keyword, &value);
    if(!status.ok())
    {
        //cerr << " No value is stored in Client Map Sigma for the keyword and store first time = " << keyword << endl;
        return 0;                                                                           // 0: There is not value stored corresponding to Keyword
    }
    else     
    { 
        // Decomposing value into st and c

        st = value.substr(0, 16);                                                           // As st is of size 16 hence we extract first 16 bytes from
        try
        {
            c = std::stoi(value.substr(16));                                                // extract left bytes and converting into int c  
        } 
        catch (const std::exception &e) 
        {
            cerr << "Error converting to c to integer: " << e.what() << endl;
        }
        return 1;                                                                           // 1: There is value stored corresponding to the queried Keyword
    }
}


void encryptAES(const SecByteBlock &key, const string &plaintext, string &ciphertext) 
{
    ECB_Mode<AES>::Encryption encryptor;
    encryptor.SetKey(key, key.size());
    StringSource(plaintext, true, new StreamTransformationFilter(encryptor, new StringSink(ciphertext), BlockPaddingSchemeDef::NO_PADDING));
}

void decryptAES(const SecByteBlock &key, const string &ciphertext, string &plaintext) 
{
    ECB_Mode<AES>::Decryption decryptor;
    decryptor.SetKey(key, key.size());
    StringSource(ciphertext, true, new StreamTransformationFilter(decryptor, new StringSink(plaintext),  BlockPaddingSchemeDef::NO_PADDING));
}

string SecByteBlockToString(const SecByteBlock& block) 
{
    string result;
    result.reserve(block.size());

    for (size_t i = 0; i < block.size(); ++i) 
    {
        result.push_back(static_cast<unsigned char>(block[i]));
    }

    return result;
}

SecByteBlock StringToSecByteBlock(const string& str) 
{
    // Create a SecByteBlock with the same size as the string
    SecByteBlock block(reinterpret_cast<const unsigned char*>(str.data()), 16);
    return block;
}



std::string xorStrings(const std::string &str1, const std::string &str2) 
{
    if (str1.size() != str2.size()) 
    {
        throw std::invalid_argument("Strings must be of equal length for XOR operation.");
    }

    std::string result;
    result.reserve(str1.size());                                                            // Reserve space to avoid reallocations

    for (size_t i = 0; i < str1.size(); ++i) 
    {
        result += static_cast<char>(str1[i] ^ str2[i]);                                     // XOR corresponding characters
    }
    return result;
}

string sha256(const std::string &input)                                                     // Function to compute SHA-256 hash and return 32 bytes as std::string
{
    SHA256 hash;
    std::string digest;                                                                     // Leave it uninitialized
    StringSource ss(input, true, new HashFilter(hash, new StringSink(digest)));             // Compute the hash

    return digest;
}

CryptoPP::SecByteBlock generateRandom128BitKey()                                           // Function to generate a random key of length 128 bit
{
    CryptoPP::AutoSeededRandomPool prng;                                                   // Random number generator

    CryptoPP::SecByteBlock key(16);                                                        // 128-bit key (16 bytes)
    prng.GenerateBlock(key, 16);                                                           // Generate random bytes

    return key;                                                                            // Return raw bytes
}

std::string generate128BitString() 
{
    std::random_device rd;                                                                 // Seed generator
    std::mt19937 gen(rd());                                                                // Mersenne Twister engine
    std::uniform_int_distribution<unsigned char> dis(0, 255);

    std::string result;
    for (int i = 0; i < 16; ++i) 
    {
        unsigned char random_char = dis(gen);
        result.push_back(static_cast<char>(random_char));                                  // Append raw byte to result
    }
    return result;
}


string hashSHAKE(const std::string& input, size_t outputLength)                            // Function to compute SHAKE-128 hash of desired length (32 bytes) as string
{
    SHAKE128 shake; // For SHAKE256, use SHAKE256 shake;
    string output;

    shake.Update(reinterpret_cast<const unsigned char*>(input.data()), input.size());
    output.resize(outputLength);
    shake.TruncatedFinal(reinterpret_cast<unsigned char*>(&output[0]), outputLength);

    return output;
}

// void display_map(rocksdb::DB* map)
// {
//     rocksdb::Iterator* it = map->NewIterator(rocksdb::ReadOptions());
//     cout << endl;
//     for (it->SeekToFirst(); it->Valid(); it->Next()) 
//     {
//         cout << " Key : " << it->key().ToString() << ", Value: " << it->value().ToString() << std::endl;
//     }

//     cout << endl;
// }

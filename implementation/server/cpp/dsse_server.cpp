#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <stdexcept>
#include <tuple>
#include <algorithm>
#include <cryptopp/base64.h>

#include "FAST.h"

using namespace std;

// Helper to encode binary data to Base64
string toBase64(const string& input) {
    string encoded;
    CryptoPP::StringSource ss(input, true,
        new CryptoPP::Base64Encoder(
            new CryptoPP::StringSink(encoded),
            false
        )
    );
    return encoded;
}

// Robustly trim ALL whitespace characters from both ends
void trim(string& s) {
    if (s.empty()) return;
    size_t first = s.find_first_not_of(" \t\r\n");
    if (first == string::npos) {
        s.clear();
        return;
    }
    size_t last = s.find_last_not_of(" \t\r\n");
    s = s.substr(first, (last - first + 1));
}

// Helper to decode Base64 strings back to binary
string fromBase64(const string& input) {
    // Before decoding, ensure the string is clean
    string cleanInput = input;
    trim(cleanInput);
    if (cleanInput.empty()) return "";
    
    string decoded;
    try {
        CryptoPP::StringSource ss(cleanInput, true,
            new CryptoPP::Base64Decoder(
                new CryptoPP::StringSink(decoded)
            )
        );
    } catch (const std::exception& e) {
        cerr << "Base64 Decoding Error: " << e.what() << " on string: [" << cleanInput << "]" << endl;
        return "";
    }
    return decoded;
}

class ServerNode : public DSSE {
public:
    void Setup(string dbPath) {
        rocksdb::Options options;
        options.create_if_missing = true;
        
        // In this integration, we only care about map2 on the server.
        // map1 is client-side.
        rocksdb::Status status = rocksdb::DB::Open(options, dbPath, &Data.map2);
        if (!status.ok()) {
            cerr << "Error opening RocksDB for map2: " << status.ToString() << endl;
            exit(1);
        }
    }
};

int main(int argc, char* argv[]) {
    if (argc < 3) {
        cerr << "Usage: " << argv[0] << " <db_path> <opcode> [args...]" << endl;
        return 1;
    }

    string dbPath = argv[1];
    int opCode = stoi(argv[2]);

    ServerNode dsse;
    dsse.Setup(dbPath);

    if (opCode == 0) { // Update
        if (argc < 5) {
            cerr << "Usage: Update requires <key> <value>" << endl;
            return 1;
        }
        string key = fromBase64(argv[3]);
        string value = fromBase64(argv[4]);
        dsse.Update_server(make_tuple(key, value));
        cout << "Update successful" << endl;
    
    } else if (opCode == 1) { // Search
        if (argc < 6) {
            cerr << "Usage: Search requires <keyword_token> <state_token> <count>" << endl;
            return 1;
        }
        string t_keyword = fromBase64(argv[3]);
        string st_c = fromBase64(argv[4]);
        int c = stoi(argv[5]);

        vector<string> results;
        dsse.Search_server(make_tuple(t_keyword, st_c, c), results);
        
        for(const auto& res : results) {
            cout << "RESULT:" << toBase64(res) << endl;
        }
    } else if (opCode == 2) { // Batch Update - read key\tvalue pairs from stdin
        string line;
        int count = 0;
        while (getline(cin, line)) {
            if (line.empty()) continue;

            size_t tab = line.find('\t');
            if (tab == string::npos) continue;

            string key = fromBase64(line.substr(0, tab));
            string value = fromBase64(line.substr(tab + 1));

            if (key.empty() || value.empty()) continue;

            dsse.Update_server(make_tuple(key, value));
            count++;
        }
        cout << "Batch update successful: " << count << " entries" << endl;
    } else {
        cerr << "Invalid OpCode" << endl;
        return 1;
    }

    return 0;
}

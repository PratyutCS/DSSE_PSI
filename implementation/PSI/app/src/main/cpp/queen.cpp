#include <iostream>
#include <vector>
#include <string>
#include <tuple>
#include <android/log.h>
#include <sys/stat.h>
#include <errno.h>
#include "v2.cpp"
#include "FAST.h"
#include "BitSequence.cpp"

#define LOG_TAG "PSI_QUEEN"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

using namespace std;

// This function calculates the update tokens for the server
vector<tuple<string, string>> queen_process(vector<tuple<string, int>> &inp, const string &storage_path, vector<string> &encrypted_paths) {
    LOGI("Queen redirecting to DBConversion. Input size: %zu", inp.size());
    
    // 1. Convert input to index/keyword pairs. This also sorts inp by keyword.
    auto result = DBConversion(inp);
    
    LOGI("DBConversion complete. Sorted input size: %zu", inp.size());

    // 2. Encrypt and rename files according to their sorted order (ID0, ID1, etc.)
    DSSE FAST_;
    FAST_.Setup(storage_path);
    SecByteBlock key = FAST_.Get_Client_sk();

    // Create encrypted directory if not exists
    string enc_dir = storage_path + "/encrypted";
    if (mkdir(enc_dir.c_str(), 0777) == -1) {
        if (errno != EEXIST) {
            LOGI("Warning: Could not create directory %s (errno: %d)", enc_dir.c_str(), errno);
        }
    }
    
    encrypted_paths.clear();
    for (size_t i = 0; i < inp.size(); i++) {
        string original_path = get<0>(inp[i]);
        
        // Extract filename from path
        size_t last_slash = original_path.find_last_of("/");
        string filename_only = (last_slash != string::npos) ? original_path.substr(last_slash + 1) : original_path;

        // Extract extension from filename
        size_t last_dot = filename_only.find_last_of(".");
        string ext = (last_dot != string::npos) ? filename_only.substr(last_dot) : "";
        
        string new_filename = "ID" + to_string(i) + ext;
        string new_path = enc_dir + "/" + new_filename;
        
        LOGI("Encrypting file %s to %s", original_path.c_str(), new_path.c_str());
        encryptFile(key, original_path, new_path);
        encrypted_paths.push_back(new_path);
    }

    // 3. Initialize DSSE to generate tokens
    vector<tuple<string, string>> u_List;

    for(size_t i=0 ; i<result.size() ; i++){
        tuple<string, string> u_token;
        string ind = to_string(get<0>(result[i]));
        string keyword = to_string(get<1>(result[i]));
        bool op = true; // Always update (add)
        
        FAST_.Update_client(ind, keyword, op, u_token);
        u_List.push_back(u_token);
    }
    
    LOGI("Token generation complete. Returning %zu tokens.", u_List.size());
    return u_List;
}

// This function generates the search token (t_keyword, st_c, c)
tuple<string, string, int> queen_search_client(string keyword, const string &storage_path) {
    DSSE FAST_;
    FAST_.Setup(storage_path); // Initialize as well.
    
    tuple<string, string, int> s_token;
    FAST_.Search_client(keyword, s_token);
    return s_token;
}

// This function performs the BitSequence post-processing
vector<int> queen_post_process(int index_range, string res1_0, string res1_1, string res2_0, string res2_1) {
    // res1_0 is equal apply check for param1? No, in the queen.cpp example:
    // search_result1[1] is the value used for less_than.
    // Let's mirror the queen.cpp main exactly.
    
    // search_resultX[0] corresponds to the "equal" index if any, [1] to the less-than boundary.
    // Wait, let's look at benchmark_pi/queen.cpp again.
    
    // stoull(search_result1[1]) is used for less_than.
    // search_result1[0] is used for equal_bits.
    
    BitSequence<uint64_t> param1_L_bitmap = less_than(index_range, stoull(res1_1));
    BitSequence<uint64_t> param2_L_bitmap = less_than(index_range, stoull(res2_1));

    BitSequence<uint64_t> param1_GE_bitmap = param1_L_bitmap.ones_complement();

    BitSequence<uint64_t> param2_E_bitmap(index_range, 0); 
    
    if(stoull(res2_0) != -1) {
        param2_E_bitmap = equal_bits(stoull(res2_1), stoull(res2_0), index_range);
    }
    
    BitSequence<uint64_t> param2_LE_bitmap = param2_L_bitmap.bitwise_or(param2_E_bitmap);
    BitSequence<uint64_t> result_bitmap = param1_GE_bitmap.bitwise_and(param2_LE_bitmap);

    vector<int> final_ids;
    for (size_t j = 0; j < index_range; ++j) {
        if (result_bitmap.get(j)) {
            final_ids.push_back((int)j);
        }
    }
    return final_ids;
}

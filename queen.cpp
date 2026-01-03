#include <iostream>
#include <vector>
#include <string>
#include <tuple>
#include <random>
#include <ctime>
#include <algorithm>
#include <chrono>
#include "./FAST/FAST.h"
#include "./v2.cpp"
#include "BitSequence.cpp"

using namespace std;

vector<tuple<string, int>> generate_random_input(int size) {
    vector<tuple<string, int>> inp;
    random_device rd;
    mt19937 rng(rd());
    uniform_int_distribution<int> dist(0, 99);

    for (int i = 0; i < size; ++i) {
        inp.push_back(make_tuple("ID" + to_string(i), dist(rng)));
    }
    shuffle(inp.begin(), inp.end(), rng);
    return inp;
}

int main(){
    cout<<"works"<<endl;

    DSSE FAST_;

    cout << "============================= PSI: Setup =============================" << endl; 
    auto start = chrono::high_resolution_clock::now();
    FAST_.Setup();
    auto end = chrono::high_resolution_clock::now();
    chrono::duration<double> elapsed = end - start;
    cout << "Setup took: " << elapsed.count() << " seconds" << endl;

    cout << "============================= PSI: Generate Random Input =============================" << endl; 
    start = chrono::high_resolution_clock::now();
    vector<tuple<string, int>> inp = generate_random_input(10);
    end = chrono::high_resolution_clock::now();
    elapsed = end - start;
    cout << "Generate Random Input took: " << elapsed.count() << " seconds" << endl;

    cout << "============================= PSI: DB Conversion =============================" << endl; 
    start = chrono::high_resolution_clock::now();
    auto res = DBConversion(inp, FAST_.Data.map3_sorted_index);
    end = chrono::high_resolution_clock::now();
    elapsed = end - start;
    cout << "DB Conversion took: " << elapsed.count() << " seconds" << endl;

    // debug print
    // for(int i=0 ; i<res.size() ; i++){
    //     cout<<i<<" - "<<get<0>(res[i])<<" : "<<get<1>(res[i])<<endl;
    // }

    cout << "============================= PSI: Update Client =============================" << endl; 
    start = chrono::high_resolution_clock::now();
    vector<tuple<string, string>> u_List;

    for(int i=0 ; i<res.size() ; i++){
        tuple<string, string> u_token;
        string ind = to_string(get<0>(res[i]));
        string keyword = to_string(get<1>(res[i]));
        bool op = true;
        FAST_.Update_client(ind, keyword, op, u_token);
        u_List.push_back(u_token);
    }
    end = chrono::high_resolution_clock::now();
    elapsed = end - start;
    cout << "Update Client takes: " << elapsed.count() << " seconds" << endl;

    cout << "============================= PSI: Update Server =============================" << endl; 
    start = chrono::high_resolution_clock::now();
    for(auto utk : u_List)
    {
        FAST_.Update_server(utk);
    }
    end = chrono::high_resolution_clock::now();
    elapsed = end - start;
    cout << "Update Server takes: " << elapsed.count() << " seconds" << endl;

    cout << "============================= PSI: Search LOOP =============================" << endl; 
    int choice = 1;
    while (choice != 0) {
        vector<string> search_result1;
        cout<<"enter search param1"<<endl;
        string param1;
        cin>>param1;
        tuple<string, string, int> s_token;
        
        auto start_search = chrono::high_resolution_clock::now();
        FAST_.Search_client(param1, s_token);
        FAST_.Search_server(s_token, search_result1);
        auto end_search = chrono::high_resolution_clock::now();
        chrono::duration<double> elapsed_search = end_search - start_search;
        cout << "Search 1 took: " << elapsed_search.count() << " seconds" << endl;
        
        vector<string> search_result2;
        cout<<"enter search param2"<<endl;
        string param2;
        cin>>param2;
        tuple<string, string, int> s_token2;
        
        start_search = chrono::high_resolution_clock::now();
        FAST_.Search_client(param2, s_token2);
        FAST_.Search_server(s_token2, search_result2);
        end_search = chrono::high_resolution_clock::now();
        elapsed_search = end_search - start_search;
        cout << "Search 2 took: " << elapsed_search.count() << " seconds" << endl;

        cout<<"============================= PSI: Search ============================="<<endl;

        //debug print
        for(int i=0 ; i<search_result1.size() ; i++){
            cout<<i<<" - "<<param1<<" : "<<search_result1[i]<<endl;
        }
        
        for(int i=0 ; i<search_result2.size() ; i++){
            cout<<i<<" - "<<param2<<" : "<<search_result2[i]<<endl;
        }

        cout<<"============================= PSI: Post Processing ============================="<<endl;
        auto start_post = chrono::high_resolution_clock::now();

        string size_str;
        FAST_.Data.map3_sorted_index->Get(rocksdb::ReadOptions(), "TOTAL_SIZE", &size_str);
        size_t total_size = stoull(size_str);

        BitSequence<uint64_t> param1_L_bitmap = less_than(total_size, stoull(search_result1[1]));
        BitSequence<uint64_t> param2_L_bitmap = less_than(total_size, stoull(search_result2[1]));

        // Final outputs
        // cout << "lessthan 1: "; param1_L_bitmap.print_range(0, total_size);
        // cout << "lessthan 2: "; param2_L_bitmap.print_range(0, total_size);

        // 1's Complement Demonstration
        BitSequence<uint64_t> param1_GE_bitmap = param1_L_bitmap.ones_complement();
        // cout << "param1 complement: "; param1_GE_bitmap.print_range(0, total_size);

        // Initialize with all zeros (n, 0) to ensure it exists even if 'equal' is not applicable
        BitSequence<uint64_t> param2_E_bitmap(total_size, 0); 
        
        if(stoull(search_result2[0]) != -1)
        {
            // cout<<"equal to applicable for param2"<<endl;
            param2_E_bitmap = equal_bits(stoull(search_result2[1]), stoull(search_result2[0]), total_size);
            // cout << "param2 equal bitmap is : "; param2_E_bitmap.print_range(0, total_size);
        }
        
        // OR Operation: Combine LessThan and Equal to get LessThanEqual
        BitSequence<uint64_t> param2_LE_bitmap = param2_L_bitmap.bitwise_or(param2_E_bitmap);
        // cout << "param2 LE (L | E) bitmap is : "; param2_LE_bitmap.print_range(0, total_size);

        // AND Operation: Intersect param1 GE and param2 LE
        BitSequence<uint64_t> result_bitmap = param1_GE_bitmap.bitwise_and(param2_LE_bitmap);
        cout << "result bitmap (GE & LE) is : "; result_bitmap.print_range(0, total_size);

        vector<string> final_ids;
        for (size_t j = 0; j < total_size; ++j) {
            if (result_bitmap.get(j)) {
                string id;
                rocksdb::Status status = FAST_.Data.map3_sorted_index->Get(rocksdb::ReadOptions(), to_string(j), &id);
                if (status.ok()) {
                    final_ids.push_back(id);
                }
            }
        }

        cout << "Final Matching IDs (from sorted_index DB): ";
        for (const auto& id : final_ids) {
            cout << id << " ";
        }
        cout << endl;
        
        auto end_post = chrono::high_resolution_clock::now();
        chrono::duration<double> elapsed_post = end_post - start_post;
        cout << "Post Processing took: " << elapsed_post.count() << " seconds" << endl;

        cout << "\nEnter 0 to exit or 1 to continue search: ";
        cin >> choice;
    }

    return 0;
}
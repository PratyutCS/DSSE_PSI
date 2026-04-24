// We are implementing our proposed DSSE scheme supporting range query.
#include "../src/Lambda.h"
#include <bitset>

#include <map>
#include <set>
#include <algorithm>

#ifndef SIZE_ID
#define SIZE_ID 100
#endif
#ifndef SIZE_KS
#define SIZE_KS 100
#endif

const int Size_ID = SIZE_ID;
const int Size_KS = SIZE_KS;

vector<string> Lambda_Search(DSSE &FAST_, vector<int> &Keyword_Space, tuple<int, int> &range_query);
bool test(const tuple<int, int> &range_query, const vector<string> &actual_ids, const map<string, int> &ground_truth);

void ind_Depadding_FAST(std::string &ind)
{
    size_t pos = ind.find_last_not_of('_');
    if (pos != std::string::npos)
        ind.erase(pos + 1);  // Keep characters up to the last non-'_'
    else
        ind.clear();  // All characters were '_'
}

bitset<Size_ID> build_lbitmap(int &w, vector<string> inds)
{
    bitset<Size_ID> bitmap; // All bits are initialized to 0
    // cout<<"size of inds is : "<<inds.size()<<endl;
    // for(auto id : inds)
    // {
    //     // ind_Depadding_FAST(id);
    //     cout << id << " ";
    // }
    // cout<<endl;

    if(w <=  Size_KS/2)
    {
        for(auto id : inds)
        {
            ind_Depadding_FAST(id);
            int index  = stoi(id);   
            // cout<<"L <= : "<<index<<" - " << index/10 << " - ";
            if(index%10 == 0){
                continue;
            } 
            index = index / 10;
            // cout<< index <<endl;    
            bitmap.flip(Size_ID - 1 - index); // Set the corresponding bit to 1. Note: most-significant bit need to be set 1                                        
        }
    }
    else
    {
        for(auto id : inds)
        {
            ind_Depadding_FAST(id);
            int index  = stoi(id);   
            // cout<<"L > : "<<index<<" - " << index/10 << " - ";
            if(index%10 == 0){
                continue;
            } 
            index = index / 10;
            // cout<< index <<endl;
            bitmap.flip(Size_ID - 1 - index);                                        // Set the corresponding bit to 1. Note: most-significant bit need to be set 1                                         
        }
        bitmap.flip();
    }
   
    return bitmap;
}

bitset<Size_ID> build_ebitmap(int &w, vector<string> inds)
{
    bitset<Size_ID> bitmap; 
    // for(auto id : inds)
    // {
    //     // ind_Depadding_FAST(id);
    //     cout << id << " ";
    // }
    // cout<<endl;

    for(auto id : inds)
    {
        ind_Depadding_FAST(id);
        int index  = stoi(id);   
        // cout<<"E : "<<index<<" - " << index/10 << " - ";
        if(index%10 == 0){
            continue;
        } 
        index = index / 10;
        // cout<< index <<endl;
        bitmap.flip(Size_ID - 1 - index);                                        // Set the corresponding bit to 1. Note: most-significant bit need to be set 1
    }
    return bitmap;
}


void Lambda_Setup(DSSE &FAST_)
{
    cout << "============================= PSI: Setup =============================" << endl; 
    
    Setup(FAST_);
    
}

void Lambda_Update(DSSE &FAST_, vector<int> &Keyword_Space, map<string, int> &ground_truth)
{
    cout << "============================= PSI: Update =============================" << endl; 
    
    long double avg_uc = 0;
    long double avg_us = 0;


    // Use a random device to seed the random number engine
    std::random_device rd; 
    std::mt19937 gen(rd());                             // Mersenne Twister engine

    // Create a uniform distribution in the given range
    std::uniform_int_distribution<> distrib(0, Size_KS-1);

    int update_token_size = 0;

    for(int i = 0; i < Size_ID; i++)
    {   
        string ind = "ID" + to_string(i);
        int w = i; //distrib(gen);
        ground_truth[ind] = w;

        vector<tuple<string, string>> U_list;                                                       // Lambda Update Token List

        auto u_c0 = chrono::high_resolution_clock::now();
        Update_client(FAST_, true, ind, i, w, Keyword_Space, U_list);
        auto u_c1 = chrono::high_resolution_clock::now();
        long double u_cd = chrono::duration_cast<chrono::microseconds>(u_c1 - u_c0).count();
        

        auto u_s0 = chrono::high_resolution_clock::now();
        Update_server(FAST_, U_list);
        update_token_size = update_token_size + U_list.size();
        auto u_s1 = chrono::high_resolution_clock::now();
        long double u_sd = chrono::duration_cast<chrono::microseconds>(u_s1 - u_s0).count();

        avg_uc = avg_uc + u_cd;
        avg_us = avg_us + u_sd; 
    }

    // --- Randomized Deletions ---
    std::random_device rd_del;
    std::mt19937 gen_del(rd_del());
    std::set<int> del_indices;
    
    // 2 before Size_ID/2
    std::uniform_int_distribution<> d_before(0, Size_ID / 2 - 1);
    while(del_indices.size() < 2) del_indices.insert(d_before(gen_del));
    
    // 2 at Size_ID/2 (50 and 51)
    del_indices.insert(Size_ID / 2);
    del_indices.insert(Size_ID / 2 + 1);
    
    // 2 after Size_ID/2
    std::uniform_int_distribution<> d_after(Size_ID / 2 + 2, Size_ID - 1);
    while(del_indices.size() < 6) del_indices.insert(d_after(gen_del));

    cout << "--------- PERFORMING RANDOM DELETIONS ----------" << endl;
    for(int idx : del_indices) {
        string del_ind = "ID" + to_string(idx);
        int del_w = idx;
        vector<tuple<string, string>> del_U_list;
        cout << "Deleting " << del_ind << " (keyword " << del_w << ")" << endl;
        Update_client(FAST_, false, del_ind, idx, del_w, Keyword_Space, del_U_list);
        Update_server(FAST_, del_U_list);
        ground_truth.erase(del_ind);
    }

    cout << "============================= Comprehensive Tests (n^2) =============================" << endl;
    int pass_count = 0;
    int total_tests = 0;
    for (int a = 0; a < Size_KS; ++a) {
        for (int b = 0; b < Size_KS; ++b) {
            total_tests++;
            tuple<int, int> q = {a, b};
            vector<string> results = Lambda_Search(FAST_, Keyword_Space, q);
            if (test(q, results, ground_truth)) {
                pass_count++;
            }
        }
    }
    cout << "\033[1;32m[PASSED]\033[0m: " << pass_count << " / " << total_tests << " tests passed." << endl;
    cout << "=====================================================================================" << endl;


    long double avg_client_time = avg_uc / Size_ID;
    long double avg_server_time = avg_us / Size_ID;

    cout << "Average client-side update time (μs): " << avg_client_time << endl;
    cout << "Average server-side update time (μs): " << avg_server_time << endl;
    cout << "Total update token size: " << update_token_size << endl;
    cout << "Average update token size: " << update_token_size / Size_ID << endl;


}   


bool test(const tuple<int, int> &range_query, const vector<string> &actual_ids, const map<string, int> &ground_truth)
{
    int a = get<0>(range_query);
    int b = get<1>(range_query);
    vector<string> expected_ids;
    for (auto const& [id, keyword] : ground_truth) {
        if (keyword >= a && keyword <= b) {
            expected_ids.push_back(id);
        }
    }
    sort(expected_ids.begin(), expected_ids.end());
    vector<string> actual_ids_sorted = actual_ids;
    sort(actual_ids_sorted.begin(), actual_ids_sorted.end());

    bool match = true;
    if (expected_ids.size() != actual_ids_sorted.size()) {
        match = false;
    } else {
        for (size_t i = 0; i < expected_ids.size(); ++i) {
            if (expected_ids[i] != actual_ids_sorted[i]) {
                match = false;
                break;
            }
        }
    }

    if (!match) {
        cout << "============================= Testing Result =============================" << endl;
        cout << "\033[1;31m[FAILED]\033[0m Search results for [" << a << ", " << b << "] do NOT match ground truth!" << endl;
        cout << "Expected IDs: ";
        for (const auto& id : expected_ids) cout << id << " ";
        cout << endl;
        cout << "Actual IDs: ";
        for (const auto& id : actual_ids_sorted) cout << id << " ";
        cout << endl;
        cout << "==========================================================================" << endl;
    }
    return match;
}

vector<string> Lambda_Search_result(const tuple<int, int> &range_query, vector<string> &res1_l, vector<string> &res1_e, vector<string> &res2_l, vector<string> &res2_e)
{
    
    // ####################### Client Side: Bitmap Generation and result interpretation #####################################################

    bitset<Size_ID> a_lbm, a_ebm;
    bitset<Size_ID> b_lbm, b_ebm;

    int a = get<0>(range_query);
    int b = get<1>(range_query);

    // As this instant, we are considering only a <= x <= b type range query. To answer such queries, we only required l-bitmap of a, and l and e bitmap of b.

    a_lbm = build_lbitmap(a, res1_l);
    //cout << "l-bitmap(" << get<0>(range_query) << ")= "<<  a_lbm << " and ";
    

    // a_ebm = build_bitmap(bitmap_length, res1_e);
    // cout << "e-bitmap(" << get<0>(range_query) << ") = " << a_ebm << endl;

    b_lbm = build_lbitmap(b, res2_l);
    //cout << "l-bitmap(" << get<1>(range_query) <<")= " << b_lbm << " and ";

    b_ebm = build_ebitmap(b, res2_e);
    //cout << "e-bitmap(" << get<1>(range_query) << ")= " << b_ebm << endl;

    //cout << "Search Result for the query q: [" << get<0>(range_query) << ", " << get<1>(range_query) << "]" << endl;
    
    auto result = ((~a_lbm) & (b_lbm | b_ebm));
    //cout << "Result: " << result << endl; // Equivalent to NOT(l-bm(a)) AND (l-bm(b) AND e-bm(b))

    // cout<<"======================================================="<<endl;
    // for(int i=0;i<SIZE_ID;i++){
    //     cout << "result is : " << result[i] << " value of index : " << i << endl;
    // }
    // cout<<"======================================================="<<endl;

    for(auto id : res1_l)
    {
        ind_Depadding_FAST(id);
        int index  = stoi(id);
        if(index%10 == 0){
            // cout << "result is : " << result[Size_ID - 1 - index/10] << " value of index : " << Size_ID - 1 - index/10 << endl;
            if(result.test(Size_ID - 1 - index/10)){
                result[Size_ID - 1 - index/10] = 0;
            }
        }
    }

    for(auto id : res1_e)
    {
        ind_Depadding_FAST(id);
        int index  = stoi(id);
        if(index%10 == 0){
            // cout << "result is : " << result[Size_ID - 1 - index/10] << " value of index : " << Size_ID - 1 - index/10 << endl;
            if(result.test(Size_ID - 1 - index/10)){
                result[Size_ID - 1 - index/10] = 0;
            }
        }
    }

    for(auto id : res2_l)
    {
        ind_Depadding_FAST(id);
        int index  = stoi(id);
        if(index%10 == 0){
            // cout << "result is : " << result[Size_ID - 1 - index/10] << " value of index : " << index/10 << endl;
            if(result.test(Size_ID - 1 - index/10)){
                result[Size_ID - 1 - index/10] = 0;
            }
        }
    }

    for(auto id : res2_e)
    {
        ind_Depadding_FAST(id);
        int index  = stoi(id);
        if(index%10 == 0){
            // cout << "result is : " << result[Size_ID - 1 - index/10] << " value of index : " << index/10 << endl;
            if(result.test(Size_ID - 1 - index/10)){
                result[Size_ID - 1 - index/10] = 0;
            }
        }
    }

    // cout<<"======================================================="<<endl;
    // for(int i=0;i<SIZE_ID;i++){
    //     cout << "result is : " << result[i] << " value of index : " << i << endl;
    // }
    // cout<<"======================================================="<<endl;

    vector<string> found_ids;
    for (int i = Size_ID - 1; i >= 0; --i) 
    {
        if (result.test(i)) 
        {
            found_ids.push_back("ID" + to_string(Size_ID - 1 - i));
            // cout << "ID" << Size_ID - 1 - i << endl;
        }
    }
    return found_ids;
}


vector<string> Lambda_Search(DSSE &FAST_, vector<int> &Keyword_Space, tuple<int, int> &range_query)
{
    // cout << "============================= PSI: Search for [" << get<0>(range_query) << ", " << get<1>(range_query) << "] =============================" << endl;

    vector<string> res1_l, res1_e, res2_l, res2_e;
    tuple<string, string, int> stk1_l, stk1_e, stk2_l, stk_2_e;                                                   // Lambda's Search Tokens

    auto s_c0 = chrono::high_resolution_clock::now();
    Search_client(FAST_, range_query, Keyword_Space, stk1_l, stk1_e, stk2_l, stk_2_e);
    auto s_c1 = chrono::high_resolution_clock::now();
    auto s_cd = chrono::duration_cast<chrono::microseconds>(s_c1 - s_c0).count();
    // cout << "Client: Search Time = " << s_cd  << " μs" << endl;

    auto s_s0 = chrono::high_resolution_clock::now();
    Search_server(FAST_, stk1_l, stk1_e, stk2_l, stk_2_e, res1_l, res1_e, res2_l, res2_e);
    auto s_s1 = chrono::high_resolution_clock::now();
    auto s_sd = chrono::duration_cast<chrono::microseconds>(s_s1 - s_s0).count();
    // cout << "Server: Search Time = " << s_sd << " μs"<< endl;

    return Lambda_Search_result(range_query, res1_l, res1_e, res2_l, res2_e);
}



int main()
{
    DSSE FAST_;

    Lambda_Setup(FAST_);

    //============================= PSI: Keyword Space =============================    

    vector<int> Keyword_Space;
    for(int i = 0; i < Size_KS; i++)
    {
        Keyword_Space.push_back(i);
    }



    map<string, int> ground_truth;
    char choice = 'y';
    int expression;
    tuple<int, int> range_query;

    while(choice == 'y' || choice == 'Y')
    {
        cout << "FAST: Enter Choice (Update/Search) : (1/2)" << endl;                 // Getting the client secret key from the DSSE class
        cin >> expression;
        switch (expression)
        {
            case 1: 
                Lambda_Update(FAST_, Keyword_Space, ground_truth);
                break;

            case 2:
                cout << "Enter the range query of the form [int a, int b]: a  = " << endl;
                cin >> get<0>(range_query);

                cout << "Enter the range query of the form [int a, int b]: b  = " << endl;
                cin >> get<1>(range_query);

                {
                    vector<string> results = Lambda_Search(FAST_, Keyword_Space, range_query);
                    if (test(range_query, results, ground_truth)) {
                        cout << "\033[1;32m[PASSED]\033[0m Test case passed." << endl;
                    }
                }
                break;
            
            default:
                cout << "The entered option is not correct" << endl;
                break;
        }
        cout << "Do you want to continue? (y/n): ";
        cin >> choice;  
    }
    
    return 0;
}

    /*
    * The search query to be performed in the very specific manner. Here is the format for the range query. 
    * q: v1 Delta1 x Delta2 v2,                                     where Deltai belongs to {<, <=, >, >=, =} 
    * q ask to find the identifiers (x) whose range value satisfies the inequalities as specified in q.
    */

    

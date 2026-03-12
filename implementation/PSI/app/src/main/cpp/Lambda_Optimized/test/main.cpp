// We are implementing our proposed DSSE scheme supporting range query.
#include "../src/Lambda.h"
#include <vector>
#include <iostream>
#include <chrono>
#include <algorithm>
#include <tuple>
#include <random>

using namespace std;

// This is now dynamic. In the real application, it equals the file count from the server.
int Size_ID = 23; 
const int Size_KS = 1000;

void ind_Depadding_FAST(std::string &ind)
{
    size_t pos = ind.find_last_not_of('_');
    if (pos != std::string::npos)
        ind.erase(pos + 1);  // Keep characters up to the last non-'_'
    else
        ind.clear();  // All characters were '_'
}

// Replaced fixed bitset with dynamic vector<bool> to support dynamic Size_ID
vector<bool> build_lbitmap(int w, const vector<string>& inds)
{
    vector<bool> bitmap(Size_ID, false);

    if(w <= Size_KS/2)
    {
        for(auto id : inds)
        {
            string depadded = id;
            ind_Depadding_FAST(depadded);
            try {
                int index = stoi(depadded);
                // In bitset logic: index 'idx' maps to position 'Size_ID - 1 - idx'
                if (index >= 0 && index < Size_ID) bitmap[Size_ID - 1 - index] = true;
            } catch(...) {}
        }
    }
    else
    {
        for(auto id : inds)
        {
            string depadded = id;
            ind_Depadding_FAST(depadded);
            try {
                int index = stoi(depadded);
                if (index >= 0 && index < Size_ID) bitmap[Size_ID - 1 - index] = true;
            } catch(...) {}
        }
        for(int i = 0; i < Size_ID; i++) bitmap[i] = !bitmap[i];
    }
   
    return bitmap;
}

vector<bool> build_ebitmap(int w, const vector<string>& inds)
{
    vector<bool> bitmap(Size_ID, false);
    for(auto id : inds)
    {
        string depadded = id;
        ind_Depadding_FAST(depadded);
        try {
            int index = stoi(depadded);
            if (index >= 0 && index < Size_ID) bitmap[Size_ID - 1 - index] = true;
        } catch(...) {}
    }
    return bitmap;
}


void Lambda_Setup(DSSE &FAST_)
{
    cout << "============================= PSI: Setup =============================" << endl; 
    Setup(FAST_);
}

void Lambda_Update(DSSE &FAST_, const vector<int> &Keyword_Space)
{
    cout << "============================= PSI: Update =============================" << endl; 
    
    long double avg_uc = 0;
    long double avg_us = 0;

    std::random_device rd; 
    std::mt19937 gen(rd());

    // Updating a subset of files for testing
    int update_count = (Size_ID < 5) ? Size_ID : 5;

    for(int i = 0; i < update_count; i++)
    {   
        string ind = "ID" + to_string(i);
        int w = i; // Keyword is same as ID for predictable testing

        vector<tuple<string, string>> U_list;

        auto u_c0 = chrono::high_resolution_clock::now();
        Update_client(FAST_, true, ind, i, w, Keyword_Space, U_list);
        auto u_c1 = chrono::high_resolution_clock::now();
        long double u_cd = chrono::duration_cast<chrono::microseconds>(u_c1 - u_c0).count();
        

        auto u_s0 = chrono::high_resolution_clock::now();
        Update_server(FAST_, U_list);
        auto u_s1 = chrono::high_resolution_clock::now();
        long double u_sd = chrono::duration_cast<chrono::microseconds>(u_s1 - u_s0).count();

        avg_uc = avg_uc + u_cd;
        avg_us = avg_us + u_sd; 
    }

    long double avg_client_time = avg_uc / Size_ID;
    long double avg_server_time = avg_us / Size_ID;

    cout << "Average client-side update time (μs): " << avg_client_time << endl;
    cout << "Average server-side update time (μs): " << avg_server_time << endl;
}   


void Lambda_Search_result(const tuple<int, int> &range_query, vector<string> &res1_l, vector<string> &res1_e, vector<string> &res2_l, vector<string> &res2_e)
{
    
    // ####################### Client Side: Bitmap Generation and result interpretation #####################################################

    int a = get<0>(range_query);
    int b = get<1>(range_query);

    vector<bool> a_lbm = build_lbitmap(a, res1_l);
    cout << "  [Search] Bitmap LT(" << a << ") : ";
    for(int i = Size_ID - 1; i >= 0; i--) cout << (a_lbm[i] ? "1" : "0");
    cout << endl;

    vector<bool> b_lbm = build_lbitmap(b, res2_l);
    cout << "  [Search] Bitmap LT(" << b << ") : ";
    for(int i = Size_ID - 1; i >= 0; i--) cout << (b_lbm[i] ? "1" : "0");
    cout << endl;

    vector<bool> b_ebm = build_ebitmap(b, res2_e);
    cout << "  [Search] Bitmap EQ(" << b << ") : ";
    for(int i = Size_ID - 1; i >= 0; i--) cout << (b_ebm[i] ? "1" : "0");
    cout << endl;

    vector<bool> result(Size_ID);
    for(int i = 0; i < Size_ID; i++) {
        // ~LT(a) is !a_lbm[i]
        result[i] = (!a_lbm[i]) && (b_lbm[i] || b_ebm[i]);
    }

    cout << "  [Search] Final Logic Result (~LT(a) & (LT(b)|EQ(b))): ";
    for(int i = Size_ID - 1; i >= 0; i--) cout << (result[i] ? "1" : "0");
    cout << endl;
    
    cout << "Matching IDs found:" << endl;
    for (int i = Size_ID - 1; i >= 0; --i) 
    {
        if (result[i]) 
        {
            cout << "-> ID" << (Size_ID - 1 - i) << endl;
        }
    }

}


void Lambda_Search(DSSE &FAST_, vector<int> &Keyword_Space, tuple<int, int> &range_query)
{
    cout << "============================= PSI: Search =============================" << endl;

    vector<string> res1_l, res1_e, res2_l, res2_e;
    tuple<string, string, int> stk1_l, stk1_e, stk2_l, stk_2_e;                                                   // Lambda's Search Tokens

    auto s_c0 = chrono::high_resolution_clock::now();
    Search_client(FAST_, range_query, Keyword_Space, stk1_l, stk1_e, stk2_l, stk_2_e);
    auto s_c1 = chrono::high_resolution_clock::now();
    auto s_cd = chrono::duration_cast<chrono::microseconds>(s_c1 - s_c0).count();
    cout << "Client: Search Time = " << s_cd  << " μs" << endl;

    auto s_s0 = chrono::high_resolution_clock::now();
    Search_server(FAST_, stk1_l, stk1_e, stk2_l, stk_2_e, res1_l, res1_e, res2_l, res2_e);
    auto s_s1 = chrono::high_resolution_clock::now();
    auto s_sd = chrono::duration_cast<chrono::microseconds>(s_s1 - s_s0).count();
    cout << "Server: Search Time = " << s_sd << " μs"<< endl;

    Lambda_Search_result(range_query, res1_l, res1_e, res2_l, res2_e);
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



    char choice = 'y';
    int expression;

    tuple<int, int> range_query;

    while(choice == 'y' || choice == 'Y')
    {
        cout << "FAST: Enter Choice (Update/Search/SetSize) : (1/2/3)" << endl;
        if (!(cin >> expression)) break;
        switch (expression)
        {
            case 1: 
                Lambda_Update(FAST_, Keyword_Space);
                break;

            case 2:
                cout << "Enter the range query of the form [int a, int b]: a  = " << endl;
                cin >> get<0>(range_query);

                cout << "Enter the range query of the form [int a, int b]: b  = " << endl;
                cin >> get<1>(range_query);

                Lambda_Search(FAST_, Keyword_Space, range_query);
                break;
            
            case 3:
                cout << "Current Size_ID is " << Size_ID << ". Enter new size: ";
                cin >> Size_ID;
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

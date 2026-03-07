// Benchmarking version of main.cpp for Lambda DSSE scheme
// Size_ID and Size_KS are provided via compiler -D flags (e.g. -DPARAM_SIZE_ID=100 -DPARAM_SIZE_KS=100)

#include "../src/Lambda.h"

#include <bitset>
#include <chrono>
#include <fstream>

// Allow override from compiler flags; default to 10 if not provided
#ifndef PARAM_SIZE_ID
#define PARAM_SIZE_ID 10
#endif

#ifndef PARAM_SIZE_KS
#define PARAM_SIZE_KS 10
#endif

const int Size_ID = PARAM_SIZE_ID;
const int Size_KS = PARAM_SIZE_KS;

// ========================= Utility Functions =========================

void ind_Depadding_FAST(std::string &ind)
{
    size_t pos = ind.find_last_not_of('_');
    if (pos != std::string::npos)
        ind.erase(pos + 1);
    else
        ind.clear();
}

bitset<PARAM_SIZE_ID> build_bitmap(vector<string> inds)
{
    bitset<PARAM_SIZE_ID> bitmap;
    for(auto id : inds)
    {
        ind_Depadding_FAST(id);
        int index = stoi(id);
        bitmap.set(PARAM_SIZE_ID - 1 - index);
    }
    return bitmap;
}


// ========================= Setup =========================
long double Lambda_Setup_Bench(DSSE &FAST_)
{
    auto t0 = std::chrono::high_resolution_clock::now();
    Setup(FAST_);
    auto t1 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<long double, std::micro> duration = t1 - t0;
    return duration.count();
}

// ========================= Update =========================
// Returns: {avg_update_client_us, avg_update_server_us}
pair<long double, long double> Lambda_Update_Bench(DSSE &FAST_, vector<int> &Keyword_Space)
{
    long double avg_uc = 0;
    long double avg_us = 0;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(0, Size_KS-1);

    for(int i = 0; i < Size_ID; i++)
    {
        string ind = "ID" + to_string(i);
        int w = i;

        vector<tuple<string, string>> U_list;

        auto u_c0 = std::chrono::high_resolution_clock::now();
        Update_client(FAST_, true, ind, i, w, Keyword_Space, U_list);
        auto u_c1 = std::chrono::high_resolution_clock::now();
        std::chrono::duration<long double, std::micro> u_cd = u_c1 - u_c0;

        auto u_s0 = std::chrono::high_resolution_clock::now();
        Update_server(FAST_, U_list);
        auto u_s1 = std::chrono::high_resolution_clock::now();
        std::chrono::duration<long double, std::micro> u_sd = u_s1 - u_s0;

        avg_uc += u_cd.count();
        avg_us += u_sd.count();
    }

    long double avg_client = avg_uc / Size_ID;
    long double avg_server = avg_us / Size_ID;

    return {avg_client, avg_server};
}

// ========================= Search + Post-processing =========================
// Returns: {avg_search_client_us, avg_search_server_us, post_processing_us}
tuple<long double, long double, long double> Lambda_Search_Bench(DSSE &FAST_, vector<int> &Keyword_Space)
{
    // Use a range query [0, Size_KS - 1] to cover the full keyword space
    tuple<int, int> range_query = make_tuple(0, Size_KS - 1);

    vector<string> res1_l, res1_e, res2_l, res2_e;
    tuple<string, string, int> stk1_l, stk1_e, stk2_l, stk_2_e;

    // ---------- Search Client ----------
    auto s_c0 = std::chrono::high_resolution_clock::now();
    Search_client(FAST_, range_query, Keyword_Space, stk1_l, stk1_e, stk2_l, stk_2_e);
    auto s_c1 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<long double, std::micro> s_cd = s_c1 - s_c0;

    // ---------- Search Server ----------
    auto s_s0 = std::chrono::high_resolution_clock::now();
    Search_server(FAST_, stk1_l, stk1_e, stk2_l, stk_2_e, res1_l, res1_e, res2_l, res2_e);
    auto s_s1 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<long double, std::micro> s_sd = s_s1 - s_s0;

    // ---------- Post-processing (bitmap construction + logical operations) ----------


    bitset<PARAM_SIZE_ID> a_lbm, a_ebm;
    bitset<PARAM_SIZE_ID> b_lbm, b_ebm;
    auto pp_t0 = chrono::high_resolution_clock::now();

    a_lbm = build_bitmap(res1_l);
    b_lbm = build_bitmap(res2_l);
    b_ebm = build_bitmap(res2_e);

    auto result = ((~a_lbm) & (b_lbm | b_ebm));

    // Iterate through result to extract matching IDs (to ensure the compiler doesn't optimize it away)
    volatile int match_count = 0;
    for (int i = PARAM_SIZE_ID - 1; i >= 0; --i)
    {
        if (result.test(i))
        {
            match_count++;
        }
    }

    auto pp_t1 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<long double, std::micro> pp_d = pp_t1 - pp_t0;

    return {s_cd.count(), s_sd.count(), pp_d.count()};
}


int main()
{
    // ========================= Setup =========================
    DSSE FAST_;
    long double setup_time = Lambda_Setup_Bench(FAST_);

    // ========================= Keyword Space =========================
    vector<int> Keyword_Space;
    for(int i = 0; i < Size_KS; i++)
    {
        Keyword_Space.push_back(i);
    }

    // ========================= Update =========================
    auto [avg_update_client, avg_update_server] = Lambda_Update_Bench(FAST_, Keyword_Space);


    // ========================= Search + Post-processing =========================
    auto [avg_search_client, avg_search_server, post_processing] = Lambda_Search_Bench(FAST_, Keyword_Space);

    // ========================= Output structured benchmark data =========================
    // Format: [BENCH] key=value (High resolution timing only)
    std::cout << "[BENCH] Setup_us=" << std::fixed << std::setprecision(4) << setup_time << std::endl;
    std::cout << "[BENCH] Avg_Update_Server_us=" << std::fixed << std::setprecision(4) << avg_update_server << std::endl;
    std::cout << "[BENCH] Avg_Update_Client_us=" << std::fixed << std::setprecision(4) << avg_update_client << std::endl;
    std::cout << "[BENCH] Avg_Search_Client_us=" << std::fixed << std::setprecision(4) << avg_search_client << std::endl;
    std::cout << "[BENCH] Avg_Search_Server_us=" << std::fixed << std::setprecision(4) << avg_search_server << std::endl;
    std::cout << "[BENCH] Post_Processing_us=" << std::fixed << std::setprecision(4) << post_processing << std::endl;

    return 0;
}

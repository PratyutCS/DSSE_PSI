#ifndef Lambda
#define Lambda

#include "../../FAST/FAST.h"

void Setup(DSSE &FAST_);

void Update_client(DSSE &FAST_, const bool &op, const string &ind, const int &ind_index, const int &keyword, const vector<int> &Keyword_Space, vector<tuple<string, string>> &U_list);
void Update_server(DSSE &FAST_, const vector<tuple <string, string>> &U_list);

void Search_client(DSSE &FAST_, const tuple<int, int> &range_query, vector<int> &Keyword_Set, tuple<string, string, int> &stk1_l, tuple<string, string, int> &stk1_e, tuple<string, string, int> &stk2_l, tuple<string, string, int> &stk2_e);
void Search_server(DSSE &FAST_, const tuple<string, string, int> &stk1_l, const tuple<string, string, int> &stk1_e, const tuple<string, string, int> &stk2_l, const tuple<string, string, int> &stk2_e, vector<string> &res1_l, vector<string> &res1_e, vector<string> &res2_l, vector<string> &res2_e);


#endif
#include "Lambda.h"


void Query_Gen(DSSE &FAST_, const string &Delta, const int &w, tuple<string, string, int> &stk_l, tuple<string, string, int> &stk_e)
{
    string keyword;                           
    
    if(Delta == "<=" || Delta == ">")
    {
        keyword = "0" + to_string(w);
        FAST_.Search_client(keyword, stk_l);
        
        keyword = "1" + to_string(w);
        FAST_.Search_client(keyword, stk_e);
    }

    else if(Delta == ">=" || Delta == "<")
    {
        keyword = "0" + to_string(w);
        FAST_.Search_client(keyword, stk_l);
    }

    else if (Delta == "=")
    {
        keyword = "1" + to_string(w);
        FAST_.Search_client(keyword, stk_e);
    }
}

void Search_client(DSSE &FAST_, const tuple<int, int> &range_query, vector<int> &Keyword_Space, tuple<string, string, int> &stk1_l, tuple<string, string, int> &stk1_e, tuple<string, string, int> &stk2_l, tuple<string, string, int> &stk2_e)
{
    int a = get<0>(range_query);

    string Delta1 = ">=";
    string Delta2 = "<=";

    int b = get<1>(range_query);    

    Query_Gen(FAST_, Delta1, a, stk1_l, stk1_e);

    Query_Gen(FAST_, Delta2, b, stk2_l, stk2_e);

}

void Search_server(DSSE &FAST_, const tuple<string, string, int> &stk1_l, const tuple<string, string, int> &stk1_e, const tuple<string, string, int> &stk2_l, const tuple<string, string, int> &stk2_e, vector<string> &res1_l, vector<string> &res1_e, vector<string> &res2_l, vector<string> &res2_e)
{
    FAST_.Search_server(stk1_l, res1_l); 

    FAST_.Search_server(stk1_e, res1_e);

    FAST_.Search_server(stk2_l, res2_l);

    FAST_.Search_server(stk2_e, res2_e);  

}
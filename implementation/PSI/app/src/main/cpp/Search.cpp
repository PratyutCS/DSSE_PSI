#include "FAST.h"


void DSSE::Search_client(const string &keyword, tuple<string, string, int> &s_token)
{

    // Client Side

    string t_keyword;
    encryptAES(this->Data.client_sk, sha256(keyword), t_keyword);                                       // size of t_keyword is 32 byte
    
    string st_c;
    int c = 0;
    int decision;

    decision = Retrive_tupple_DB(this->Data.map1, keyword, st_c, c);                                    // Retrieveing the value stored (st, c) corresponding to the "keyword" in the database map1 if exists
    
    if(decision == 0)
    {        
        return;                                                                                         // returning the empty search token
    }

    get<0>(s_token) = t_keyword;
    get<1>(s_token) = st_c;
    get<2>(s_token) = c;
}
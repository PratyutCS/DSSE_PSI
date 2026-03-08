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

void DSSE::Search_server(const tuple<string, string, int> &s_token, vector<string> &search_result)
{
    search_result.clear();

    vector<string> Delta;
    
    string t_keyword;
    t_keyword = get<0>(s_token);

    string st_c;
    st_c = get<1>(s_token);

    int c;
    c = get<2>(s_token);

    for(int i = c; i > 0; --i)
    {  
        string u, e;

        u = sha256(t_keyword + st_c);                                                                           // size of u is 32 byte

        rocksdb::Status status = this->Data.map2->Get(rocksdb::ReadOptions(), u, &e);                           // size of e is 32 byte

        string temp = hashSHAKE(t_keyword + st_c, 32);                                                          // as the size  = 32
        
        string ind_op_k_i = xorStrings(e, temp);

        string ind = ind_op_k_i.substr(0, 15);                                                                  // Extracting ind and op from ind||op
        string op = ind_op_k_i.substr(15, 1);  
        string k_i = ind_op_k_i.substr(16, 16);
        
        if(op == "0")
        {
            Delta.push_back(ind);
        }
        else if(op == "1")
        {
            if(find(Delta.begin(), Delta.end(), ind) != Delta.end())                                            // if ind belongs to Delta
            {
                Delta.erase(find(Delta.begin(), Delta.end(), ind));                                             // then remove it from Delta
            }
            else
            {
                search_result.push_back(ind);  
            }
        }

        string st_i_minus_1;

        decryptAES(StringToSecByteBlock(k_i), st_c, st_i_minus_1);

        st_c = st_i_minus_1;
    }

}
#include "Lambda.h"

void ind_Padding_FAST(string &ind)
{
    int padding_size = 15 - ind.size();
        if(padding_size < 0)
        {
            cerr <<"Prefix exceeds the target size for ID" << endl;
        }
    ind = ind + string(padding_size, '_');
}



void Update_client(DSSE &FAST_, const bool &op, const string &ind, const int &ind_index, const int &keyword, const vector<int> &Keyword_Space, vector<tuple<string, string>> &U_list)
{
        
    int m = Keyword_Space.size();                   

    string Lambda_ind = to_string(ind_index);                           // this is the value of psi(ind) =  ind_index
    ind_Padding_FAST(Lambda_ind);

    for (int i = 0; i < m; i++)
    {
        string Lambda_keyword = "0" + to_string(Keyword_Space[i]);

        tuple<string, string> u_token;                                  // FAST's Update tokens

        if(op == true)
        {  
            
            if(keyword < Keyword_Space[i])
            {
                FAST_.Update_client(Lambda_ind, Lambda_keyword, op, u_token);
                U_list.push_back(u_token);
            }
        }
        else if(op == false)
        {
            FAST_.Update_client(Lambda_ind, Lambda_keyword, op, u_token);
            // DB_Size = DB_Size - 1;
            U_list.push_back(u_token);
        }
    }

    tuple<string, string> u_token;                                      // FAST's Update tokens
    string Lambda_keyword = "1" + to_string(keyword);

    FAST_.Update_client(Lambda_ind, Lambda_keyword, op, u_token);
    U_list.push_back(u_token);

    // Send U_list to the server
}

void Update_server(DSSE &FAST_, vector<tuple<string, string>> &U_list)
{
    for(auto utk : U_list)
    {
        FAST_.Update_server(utk);
    }
}

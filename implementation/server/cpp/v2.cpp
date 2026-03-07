#include <iostream>
#include <vector>
#include <string>
#include <tuple>
#include <algorithm>

using namespace std;

static const int RANGE = 100000;

vector<tuple<int, int>> esGen(vector<tuple<string, int>> &inp)
{
    vector<tuple<int, int>> res(RANGE, make_tuple(0, -1));

    if (inp.empty()) {
        return res;
    }

    sort(inp.begin(), inp.end(),
    [](const tuple<string,int>& a,
        const tuple<string,int>& b) {
        return get<1>(a) < get<1>(b);
    });

    // debug print
    // for(int i=0 ; i<inp.size() ; i++)
    // {
    //     cout<<i<<" - "<<get<0>(inp[i])<<" : "<<get<1>(inp[i])<<endl;
    // }

    int match = get<1>(inp[0]);
    get<0>(res[match]) = 0;
    int i = 1;
    for(; i<inp.size() ; i++){
        if(match != get<1>(inp[i])){
            get<1>(res[match]) = i-1;
            match = get<1>(inp[i]);
            get<0>(res[match]) = i;
        }
    }
    get<1>(res[match]) = i-1;

    //debug print 

    // for(int i=0 ; i<RANGE ; i++){
    //     cout<<i<<" - "<<get<0>(res[i])<<" : "<<get<1>(res[i])<<endl;
    // }

    bool flag = true;
    for(int i=1 ; i<RANGE ; i++){
        if(get<0>(res[i]) == 0 && get<1>(res[i]) == -1){
            if(flag){
                get<0>(res[i]) = get<1>(res[i-1])+1;
                flag = false;
            }
            else{
                get<0>(res[i]) = get<0>(res[i-1]);
            }
        }
        else{
            flag = true;
        }
    }


    //debug print

    // cout<<"\nafter post processing\n"<<endl;
    
    // for(int i=0 ; i<RANGE ; i++){
    //     cout<<i<<" - "<<get<0>(res[i])<<" : "<<get<1>(res[i])<<endl;
    // }

    return res;
}

vector<tuple<int, int>> DBConversion(vector<tuple<string, int>> &inp){
    vector<tuple<int, int>> res;
    res.reserve(RANGE*2);

    auto res1 = esGen(inp);

    //debug print
    // for(int i=0 ; i<res1.size() ; i++){
    //     cout<<i<<" - "<<get<0>(res1[i])<<" : "<<get<1>(res1[i])<<endl;
    // }

    for(int i=0 ; i<RANGE ; i++){
        res.emplace_back(get<0>(res1[i]), i);
        res.emplace_back(get<1>(res1[i]), i);
    }

    return res;
}

// int main() {
//     vector<tuple<string, int>> inp = {
//         {"ID1", 22},
//         {"ID2", 20},
//         {"ID3", 18},
//         {"ID4", 25},
//         {"ID5", 30},
//         {"ID6", 22},
//         {"ID7", 18}
//     };
//     auto res = DBConversion(inp);

//     //debug print
//     for(int i=0 ; i<res.size() ; i++){
//         cout<<i<<" - "<<get<0>(res[i])<<" : "<<get<1>(res[i])<<endl;
//     }
//     return 0;
// }
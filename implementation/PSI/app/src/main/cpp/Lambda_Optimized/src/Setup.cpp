#include "Lambda.h"

void Setup(DSSE &FAST_)
{
    // Line 1

    FAST_.Setup();                                 // Creates our Processed database by calling FAST.Setup() and FAST.Update()
    
    // Line 2
    cout << FAST_.Data.map1 << endl;
    
    // Line 4 of pseudocode
    cout << "Lambda(FAST) Optimized: The EDB is : " << endl;
    cout << FAST_.Data.map2 << endl;

    cout << "Lambda Optimized: Setup Protocol Ends Here." << endl;
};
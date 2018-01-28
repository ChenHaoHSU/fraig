/****************************************************************************
  FileName     [ cirSim.cpp ]
  PackageName  [ cir ]
  Synopsis     [ Define cir simulation functions ]
  Author       [ Chung-Yang (Ric) Huang ]
  Copyright    [ Copyleft(c) 2008-present LaDs(III), GIEE, NTU, Taiwan ]
****************************************************************************/

#include <fstream>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cassert>
#include "cirMgr.h"
#include "cirGate.h"
#include "util.h"

using namespace std;

// TODO: Keep "CirMgr::randimSim()" and "CirMgr::fileSim()" for cir cmd.
//       Feel free to define your own variables or functions

/*******************************/
/*   Global variable and enum  */
/*******************************/

/**************************************/
/*   Static varaibles and functions   */
/**************************************/

/************************************************/
/*   Public member functions about Simulation   */
/************************************************/
void
CirMgr::randomSim()
{
}

void
CirMgr::fileSim(ifstream& patternFile)
{
   // Trivial case (no PI, no Simulation)
   if (_nPI == 0) return;


   // Load patternFile
   vector<string> vPatternStrings;
   if (!loadPatternFile(patternFile, vPatternStrings)) {
      cout << "0 patterns simulated.\n";
      return;
   }

   // Set PI values
   unsigned i, j;
   for (i = 0; i < vPatternStrings.size(); ++i) {
      for (j = 0; j < vPatternStrings[i].length(); ++j) {
         if (vPatternStrings[i][j] == '0')
            pi(j)->addPattern0();
         else 
            pi(j)->addPattern1();
      }
   }

   

   cout << vPatternStrings.size() << " patterns simulated.\n";
}

/*************************************************/
/*   Private member functions about Simulation   */
/*************************************************/
bool
CirMgr::loadPatternFile(ifstream& patternFile, vector<string>& vPatternStrings)
{
   // Error Handling:
   //    1. Length of pattern string == nPI
   //    2. Pattern string consists of '0' or '1'.
   // 
   vPatternStrings.clear();
   string patternStr;
   unsigned i;
   while (getline(patternFile, patternStr, '\n')) {
      // 1. Length check
      if (patternStr.length() != _nPI) {
         cerr << "\nError: Pattern(" << patternStr << ") length(" << patternStr.size() 
              << ") does not match the number of inputs(" << _nPI << ") in a circuit!!\n";
         return false;
      }
      // 2. Bit check
      for (i = 0; i < patternStr.length(); ++i) {
         if (patternStr[i] != '0' && patternStr[i] != '1') {
            cerr << "Error: Pattern(" << patternStr << ") contains a non-0/1 character(\'" 
                 << patternStr[i] << "\').";
            return false;
         }
      } 
      // Collect pattern
      vPatternStrings.emplace_back(patternStr);
   }
}







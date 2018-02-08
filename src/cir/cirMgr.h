/****************************************************************************
  FileName     [ cirMgr.h ]
  PackageName  [ cir ]
  Synopsis     [ Define circuit manager ]
  Author       [ Chung-Yang (Ric) Huang ]
  Copyright    [ Copyleft(c) 2008-present LaDs(III), GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef CIR_MGR_H
#define CIR_MGR_H

#include <vector>
#include <list>
#include <string>
#include <fstream>
#include <iostream>

#include "cirDef.h"
#include "cirGate.h"
#include "cirModel.h"
#include "cirFecGrp.h"
#include "cirSimValue.h"
#include "sat.h"

using namespace std;

// TODO: Feel free to define your own classes, variables, or functions.
extern CirMgr *cirMgr;

class CirMgr
{
public:
   CirMgr() : _bFirstSim(false) {}
   ~CirMgr() { clear(); } 

   // Access functions
   // return '0' if "gid" corresponds to an undefined gate.
   CirGate* getGate(unsigned gid) const { return _vAllGates[gid]; }

   // Member functions about circuit construction
   bool readCircuit(const string&);

   // Member functions about circuit optimization
   void sweep();
   void optimize();

   // Member functions about simulation
   void randomSim();
   void fileSim(ifstream&);
   void setSimLog(ofstream *logFile) { _simLog = logFile; }

   // Member functions about fraig
   void strash();
   void printFEC() const;
   void fraig();

   // Member functions about circuit reporting
   void printSummary() const;
   void printNetlist() const;
   void printPIs() const;
   void printPOs() const;
   void printFloatGates() const;
   void printFECPairs() const;
   void writeAag(ostream&) const;
   void writeGate(ostream&, CirGate*) const;

private:
   // Basic Info (M I L O A)
   unsigned           _maxIdx;          // M (Max gate index)
   unsigned           _nPI;             // I (Number of PIs)
   unsigned           _nLATCH;          // L (Number of LATCHes)
   unsigned           _nPO;             // O (Number of POs)
   unsigned           _nAIG;            // A (Number of AIGs)
   unsigned           _nDfsAIG;         // number of Aig in dfs list

   // Gate Lists
   vector<CirGate*>   _vPi;             // List of all PIs
   vector<CirGate*>   _vAllGates;       // List of all gates!! Can be access by idx!!
   vector<CirGate*>   _vDfsList;        // Depth-Fisrt Search List
   vector<CirGate*>   _vFloatingList;   // List of all floating gates
   vector<CirGate*>   _vUnusedList;     // List of all unused gates
   vector<CirGate*>   _vUndefList;      // List of all undefined gates

   // Sim log file (Do not remove it!!)
   ofstream          *_simLog;          // Log file of Simulation result

   // Simulation info
   bool               _bFirstSim;       // Is the FEC group be initialized ? (i.e. ever simulated?)
   list<CirFecGrp*>   _lFecGrps;        // List of all FEC groups

   // Fraig
   SatSolver          _satSolver;       // Sat solver for proving
   vector<CirGate*>   _vSatVarTable;    // Look up table (sat var -> gate)

   ////////////////////////////////////
   //      Private Functions         //
   ////////////////////////////////////

   // Private access functions
   unsigned   nPi()                const { return _nPI; }
   unsigned   nPo()                const { return _nPO; }
   CirPiGate* pi(const int i)      const { assert(0 <= i && i < (int)_nPI); return (CirPiGate*)_vPi[i];                     }
   CirPoGate* po(const int i)      const { assert(0 <= i && i < (int)_nPO); return (CirPoGate*)_vAllGates[_maxIdx + i + 1]; }
   CirPiGate* pi(const unsigned i) const { assert(i < _nPI); return (CirPiGate*)_vPi[i];                     }
   CirPoGate* po(const unsigned i) const { assert(i < _nPO); return (CirPoGate*)_vAllGates[_maxIdx + i + 1]; }
   CirGate*   constGate()          const { return _vAllGates[0]; } 

   // Private functions about parsing AAG file
   bool parseAag(ifstream&);
   bool parsePi(ifstream&);
   bool parsePo(ifstream&);
   bool parseAig(ifstream&);
   bool parseSymbol(ifstream&);
   bool parseComment(ifstream&);
   void preProcess();
   CirGate* queryGate(const unsigned);

   // Private functions about gate lists
   void buildDfsList();
   void buildFloatingList();
   void buildUnusedList();
   void buildUndefList();
   void countAig();
   void rec_dfs(CirGate*);

   // Private common functions
   void clear();
   void delGate(CirGate*);
   void sortAllGateFanout();
   void mergeGate(CirGate* liveGate, CirGate* deadGate, bool invMerged);

   // Private functions about cirSweep and cirOptimize
   
   // Private functions about cirSimulation
   bool checkPattern(const string& patternStr);
   void setPiValue(const CirModel& model);
   void simulation();
   void initClassifyFecGrp();
   void classifyFecGrp();
   void sweepInvalidFecGrp();
   void sortFecGrps_var();
   void linkGrp2Gate();
   void writeSimLog(const unsigned) const;

   // Private functions about cirFraig
   void genProofModel();
   void assignDfsOrder();
   void sortFecGrps_dfsOrder();
};

#endif // CIR_MGR_H

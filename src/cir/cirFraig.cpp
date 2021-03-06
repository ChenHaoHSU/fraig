/****************************************************************************
  FileName     [ cirFraig.cpp ]
  PackageName  [ cir ]
  Synopsis     [ Define cir FRAIG functions ]
  Author       [ Chung-Yang (Ric) Huang ]
  Copyright    [ Copyleft(c) 2012-present LaDs(III), GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cassert>
#include "cirMgr.h"
#include "cirGate.h"
#include "sat.h"
#include "myHashSet.h"
#include "myHashMap.h"
#include "util.h"
#include "cirStrash.h"

using namespace std;

// TODO: Please keep "CirMgr::strash()" and "CirMgr::fraig()" for cir cmd.
//       Feel free to define your own variables or functions

/*******************************/
/*   Global variable and enum  */
/*******************************/

extern unsigned globalRef;

/**************************************/
/*   Static varaibles and functions   */
/**************************************/

/*******************************************/
/*   Public member functions about fraig   */
/*******************************************/
// _floatList may be changed.
// _unusedList and _undefList won't be changed
void
CirMgr::strash()
{
   /****************************/
   /*  HashMap Implementation  */
   /****************************/
   HashMap<CirStrashM, CirGate*> hashM;
   hashM.init(getHashSize(_vDfsList.size()));

   CirStrashM keyM;     // hash key
   CirGate* valueM = nullptr; // value
   for (unsigned i = 0, n = _vDfsList.size(); i < n; ++i) {
      if (!_vDfsList[i]->isAig()) continue; // Skip non-AIG gate
      keyM.setGate(_vDfsList[i]);
      if (hashM.check(keyM, valueM)) {
         fprintf(stdout, "Strashing: %d merging %d...\n", 
            valueM->var(), _vDfsList[i]->var());
         mergeGate(valueM, _vDfsList[i], false);
      }
      else 
         hashM.forceInsert(keyM, _vDfsList[i]);
   }

   /****************************/
   /*  HashSet Implementation  */
   /****************************/
/*
   HashSet<CirStrashS> hashS;
   hashS.init(getHashSize(_vDfsList.size()));

   CirStrashS queryS;
   for (unsigned i = 0, n = _vDfsList.size(); i < n; ++i) {
      if (!_vDfsList[i]->isAig()) continue; // Skip non-AIG gate
      queryS.setGate(_vDfsList[i]);
      if (hashS.query(queryS)) {
         fprintf(stdout, "Strashing: %d merging %d...\n", 
            queryS.gate()->var(), _vDfsList[i]->var());
         mergeGate(queryS.gate(), _vDfsList[i], false);
      }
      else 
         hashS.insert(queryS);
   }
*/
   // Update Lists
   buildDfsList();
   buildFloatingList();
   countAig();
}

void
CirMgr::fraig()
{
   SatSolver satSolver;
   CirModel model(_nPI);
   unsigned periodCnt = 0;
   vector<pair<CirGateV, CirGateV> > vMergePairs;

   // Tuned parameter 'unsat_merge_ratio' and 'unsat_merge_ratio_increment':
   //    Only when dfs_ratio > unsat_merge_ratio will the merge operation be performed.
   double unsat_merge_ratio = 0.3;
   double unsat_merge_ratio_increment = 0.9;

   // While _lFecGrps is NOT empty, use SATsolver to prove gate equivalence in each fecgrp
   while (!_lFecGrps.empty()) {

      // Pre-process
      //    1. Initialize satSolver: reset + newVar
      //    2. Refine FEC grps: no nullptr in any fec grp
      //    3. Assign DFS order: each gate is assigned their dfsOrder
      //    4. Sort FEC grp: sort by dfsOrder so the first gate can merge every gate in its fec grp
      //
      fraig_initSatSolver(satSolver);
      fraig_refineFecGrp();
      fraig_assignDfsOrder();
      fraig_sortFecGrps_dfsOrder();
      
      // Iterate the dfsList (from PI to PO)
      for (unsigned dfsId = 0, dfsSize = _vDfsList.size(); dfsId < dfsSize; ++dfsId) {
         // Current gate
         CirGate* curGate = _vDfsList[dfsId];

         // Skip non-AIG gates
         if (!curGate->isAig()) continue;

         // Add curGate (must be AIG gate) to SATsolver
         satSolver.addAigCNF(fraig_sat_var(curGate->var()),
                             fraig_sat_var(curGate->fanin0_var()), curGate->fanin0_inv(), 
                             fraig_sat_var(curGate->fanin1_var()), curGate->fanin1_inv());

         // Skip functionally unique gates
         if (curGate->grp() == nullptr) continue;

         // Check if rep gate is curGate itself => no need to check
         CirFecGrp* fecGrp = curGate->grp();
         CirGate* repGate = fecGrp->repGate();
         if (repGate == curGate) continue;
         assert(fecGrp->candGate(curGate->grpIdx()) == curGate);

         // Current gate and representive gate of the corresponding fecgrp
         // 1. If curGate == repGate (UNSAT), then merge curGate (dead) to repGate(alive)
         // 2. If curGate != repGate (SAT), then collect the counterexample provided by SATsolver
         // 
         const CirGateV& repGateV = fecGrp->rep();
         const CirGateV& curGateV = fecGrp->cand(curGate->grpIdx());
         assert(repGateV.gate() != curGateV.gate());

         // Use SATsolver to prove if repGate and curGate are equivalent
         const bool result = fraig_prove(repGateV, curGateV, satSolver);

         /* 
          *  UNSAT:
          *  repGate and curGate are functionally equivalent 
          */
         if (!result) {
            // Record the merge pair, lazy merge
            vMergePairs.emplace_back(repGateV, curGateV); // repGateV alive; curGateV dead
            fecGrp->lazy_delete(curGate->grpIdx());
            const double current_dfs_ratio = ((double)dfsId) / ((double)dfsSize);
            if (current_dfs_ratio > unsat_merge_ratio && !vMergePairs.empty()) {
               fraig_mergeEquivalentGates(vMergePairs);
               fraig_refineFecGrp();
               fraig_printMsg_update_unsat();
               unsat_merge_ratio = std::min(1.00, unsat_merge_ratio + unsat_merge_ratio_increment);
               break;
            }
         }
         /* 
          *  SAT:
          *  repGate and curGateare are functionally INequivalent 
          */
         else { // result == true
            // Collect the assignments in SATsolver, which can separate the pair (curGate, repGate)
            fraig_collectConuterExample(satSolver, model, periodCnt++);

            // Simulate the circuit if SIM_CYCLE(64) patterns are already collected
            if (periodCnt >= SIM_CYCLE) {
               sim_simulation(model);
               sim_linkGrp2Gate();
               fraig_printMsg_update_sat();
               periodCnt = 0;
            }
         }
      } // end FOR dfs
      buildDfsList();
   } // end WHILE (!_lFecGrps.empty())

   // Final step
   //   1. If there are any gates in vMergePairs, just merge them
   //   2. Refine FEC grps
   //   3. Build DFS list
   //   4. If there are any patterns in model, just simulate the circuit by those patterns
   // 
   fraig_mergeEquivalentGates(vMergePairs);
   fraig_printMsg_update_unsat();
   buildDfsList();
   fraig_refineFecGrp();
   sim_simulation(model);
   fraig_printMsg_update_sat();
   
   // Post-process
   //   1. Do strash
   //   2. Reset _bFirstSim
   strash();
   _bFirstSim = false;
   assert(_lFecGrps.empty());
}

/********************************************/
/*   Private member functions about fraig   */
/********************************************/
void
CirMgr::fraig_initSatSolver(SatSolver& satSolver) const
{
   // Initialize SAT solver, the number of var in SAT solver == the number of _vAllGates 
   // Also, var in Minisat satSolver starts from 1 !!!! 
   // Thus, gate.var <--> SATsolver[gate.var+1]
   //        e.g. for gate 5, its variable id is (5+1) in SAT solver
   //
   satSolver.initialize();
   for (unsigned i = 0, n = _vAllGates.size(); i < n; ++i)
      satSolver.newVar();

   // Constant gate assert property false
   satSolver.assertProperty(fraig_sat_var(constGate()->var()), false);
}

void
CirMgr::fraig_assignDfsOrder()
{
   for (unsigned i = 0, n = _vDfsList.size(); i < n; ++i) {
      if (_vDfsList[i]->isAig())
         _vDfsList[i]->setDfsOrder(i + 1);
   }
   constGate()->setDfsOrder(0); // important!! No one can merge const gate!!
}

void
CirMgr::fraig_sortFecGrps_dfsOrder()
{
   for(CirFecGrp* grp : _lFecGrps)
      grp->sortDfsOrder();
   sim_linkGrp2Gate();
}

bool 
CirMgr::fraig_prove(const CirGateV& g1, const CirGateV& g2, SatSolver& satSolver)
{
   Var newV = satSolver.newVar();
   satSolver.addXorCNF(newV, fraig_sat_var(g1.gate()->var()), g1.isInv(),
                             fraig_sat_var(g2.gate()->var()), g2.isInv());
   fraig_printMsg_proving(g1, g2);
   satSolver.assumeRelease();
   satSolver.assumeProperty(newV, true);
   return satSolver.assumpSolve();
}

void
CirMgr::fraig_collectConuterExample(const SatSolver& satSolver, CirModel& model, const unsigned pos)
{
   for (unsigned i = 0; i < _nPI; ++i) {
      const int val = satSolver.getValue(fraig_sat_var(pi(i)->var()));
      if (val == 0)
         model.add0(i, pos);
      else if (val == 1)
         model.add1(i, pos);
      else assert(false); // should not return -1...
   }
}

void
CirMgr::fraig_mergeEquivalentGates(vector<pair<CirGateV, CirGateV> >& vMergePairs)
{
   // pair<CirGateV, CirGateV> : pair<aliveGate, deadGate>
   bool inv;
   CirGate *aliveGate, *deadGate;
   for (unsigned i = 0, n = vMergePairs.size(); i < n; ++i) {
      aliveGate = vMergePairs[i].first.gate();
      deadGate = vMergePairs[i].second.gate();
      inv = vMergePairs[i].first.isInv() ^ vMergePairs[i].second.isInv();
      fprintf(stdout, "Fraig: %u merging %s%u...\n", aliveGate->var(), (inv ? "!" : ""), deadGate->var());
      mergeGate(aliveGate, deadGate, inv);
      assert(vMergePairs[i].first.gate()->dfsOrder() < vMergePairs[i].second.gate()->dfsOrder());
   }
   vMergePairs.clear();
}

void
CirMgr::fraig_refineFecGrp() {
   for (CirFecGrp* grp : _lFecGrps)
      grp->refine();
   fraig_sweepInvalidFecGrp();
}

void
CirMgr::fraig_sweepInvalidFecGrp()
{
   // Remove invalid FEC groups (i.e. size < 2)
   for (auto iter = _lFecGrps.begin(); iter != _lFecGrps.end();) {
      CirFecGrp* grp = *iter;
      if (grp->isValid()) ++iter;
      else {
         delete *iter;
         iter = _lFecGrps.erase(iter);
      }
   }
}

void
CirMgr::fraig_printMsg_proving(const CirGateV& g1, const CirGateV& g2) const
{
   const bool inv = g1.isInv() ^ g2.isInv();
   if(g1.gate() == constGate())
      fprintf(stdout, "Prove %s%u = 1...", (inv ? "!" : ""), g2.gate()->var());
   else
      fprintf(stdout, "Prove (%u, %s%u)...", g1.gate()->var(), (inv ? "!" : ""), g2.gate()->var());
   cout << flush << "\r                                  \r";
}

void
CirMgr::fraig_printMsg_update_unsat() const {
   fprintf(stdout, "Updating by UNSAT... Total #FEC Group = %lu\n", _lFecGrps.size());
}

void
CirMgr::fraig_printMsg_update_sat() const {
   fprintf(stdout, "Updating by SAT... Total #FEC Group = %lu\n", _lFecGrps.size());
}

unsigned
CirMgr::fraig_sat_var(const unsigned gate_var) const {
   return gate_var + 1;
}
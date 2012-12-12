#ifndef ANALYZER_H
#define ANALYZER_H

/*************************************************************
 * Copyright: (C) 2012 by Markus Schordan                    *
 * Author   : Markus Schordan                                *
 * License  : see file LICENSE in the CodeThorn distribution *
 *************************************************************/

#include <iostream>
#include <fstream>
#include <set>
#include <string>
#include <sstream>

#include <omp.h>

#include "rose.h"

#include "AstTerm.h"
#include "Labeler.h"
#include "CFAnalyzer.h"
#include "RoseAst.h"
#include "SgNodeHelper.h"
#include "ExprAnalyzer.h"
#include "StateRepresentations.h"

// we use INT_MIN, INT_MAX
#include "limits.h"

using namespace std;

namespace CodeThorn {

#define DEBUGPRINT_STMT 0x1
#define DEBUGPRINT_STATE 0x2
#define DEBUGPRINT_STATEMOD 0x4
#define DEBUGPRINT_INFO 0x8

class AstNodeInfo : public AstAttribute {
 public:
 AstNodeInfo():label(0),initialLabel(0){}
  string toString() { stringstream ss;
	ss<<"\\n lab:"<<label<<" ";
	ss<<"init:"<<initialLabel<<" ";
	ss<<"final:"<<finalLabelsSet.toString();
	return ss.str(); 
  }
  void setLabel(Label l) { label=l; }
  void setInitialLabel(Label l) { initialLabel=l; }
  void setFinalLabels(LabelSet lset) { finalLabelsSet=lset; }
 private:
  Label label;
  Label initialLabel;
  LabelSet finalLabelsSet;
};

typedef list<const EState*> EStateWorkList;

class Analyzer {
  friend class Visualizer;

 public:
  
  Analyzer();
  ~Analyzer();

  void initAstNodeInfo(SgNode* node);

  static string nodeToString(SgNode* node);
  void initializeSolver1(std::string functionToStartAt,SgNode* root);

  PState analyzeAssignRhs(PState currentPState,VariableId lhsVar, SgNode* rhs,ConstraintSet& cset);
  EState analyzeVariableDeclaration(SgVariableDeclaration* nextNodeToAnalyze1,EState currentEState, Label targetLabel);
  list<EState> transferFunction(Edge edge, const EState* estate);
  void addToWorkList(const EState* estate);
  const EState* addToWorkListIfNew(EState estate);
  void recordTransition(const EState* sourceEState, Edge e, const EState* targetEState);
  void printStatusMessage(bool);
  bool isLTLRelevantLabel(Label label);
  set<const EState*> nonLTLRelevantEStates();
  bool isTerminationRelevantLabel(Label label);
  const EState* takeFromWorkList();

  // 5 experimental functions
  void semanticFoldingOfTransitionGraph();
  bool checkEStateSet();
  bool isConsistentEStatePtrSet(set<const EState*> estatePtrSet);
  bool isInWorkList(const EState* estate);
  bool checkTransitionGraph();

 private:
  /*! if state exists in stateSet, a pointer to the existing state is returned otherwise 
	a new state is entered into stateSet and a pointer to it is returned.
  */
  const PState* processNew(PState& s);
  const PState* processNewOrExisting(PState& s);
  const EState* processNew(EState& s);
  const EState* processNewOrExisting(EState& s);

  EStateSet::ProcessingResult process(EState& s);
  EStateSet::ProcessingResult process(Label label, PState pstate, ConstraintSet cset, InputOutput io);
  const ConstraintSet* processNewOrExisting(ConstraintSet& cset);

  EState createEState(Label label, PState pstate, ConstraintSet cset);
  EState createEState(Label label, PState pstate, ConstraintSet cset, InputOutput io);

 public:
  bool isEmptyWorkList();
  const EState* topWorkList();
  const EState* popWorkList();
  SgNode* getCond(SgNode* node);
  void generateAstNodeInfo(SgNode* node);

  //! requires init
  void runSolver1();
  void runSolver2();

  //! The analyzer requires a CFAnalyzer to obtain the ICFG.
  void setCFAnalyzer(CFAnalyzer* cf) { cfanalyzer=cf; }
  CFAnalyzer* getCFAnalyzer() const { return cfanalyzer; }

  // access  functions for computed information
  VariableIdMapping* getVariableIdMapping() { return &variableIdMapping; }
  Labeler* getLabeler() const { return cfanalyzer->getLabeler(); }
  Flow* getFlow() { return &flow; }
  PStateSet* getPStateSet() { return &pstateSet; }
  EStateSet* getEStateSet() { return &estateSet; }
  TransitionGraph* getTransitionGraph() { return &transitionGraph; }
  ConstraintSetMaintainer* getConstraintSetMaintainer() { return &constraintSetMaintainer; }

  //private: TODO
  Flow flow;
  SgNode* startFunRoot;
  CFAnalyzer* cfanalyzer;

  //! compute the VariableIds of variable declarations
  set<VariableId> determineVariableIdsOfVariableDeclarations(set<SgVariableDeclaration*> decls);
  //! compute the VariableIds of SgInitializedNamePtrList
  set<VariableId> determineVariableIdsOfSgInitializedNames(SgInitializedNamePtrList& namePtrList);

  set<string> variableIdsToVariableNames(set<VariableId>);

  //bool isAssertExpr(SgNode* node);
  bool isFailedAssertEState(const EState* estate);
  //! adds a specific code to the io-info of an estate which is checked by isFailedAsserEState and determines a failed-assert estate. Note that the actual assert (and its label) is associated with the previous estate (this information can therefore be obtained from a transition-edge in the transition graph).
  EState createFailedAssertEState(const EState estate, Label target);
  //! list of all asserts in a program
  list<SgNode*> listOfAssertNodes(SgProject *root);
  //! rers-specific error_x: assert(0) version 
  list<pair<SgLabelStatement*,SgNode*> > listOfLabeledAssertNodes(SgProject *root);
  void initLabeledAssertNodes(SgProject* root) {
	_assertNodes=listOfLabeledAssertNodes(root);
  }
  string labelNameOfAssertLabel(Label lab) {
	string labelName;
	for(list<pair<SgLabelStatement*,SgNode*> >::iterator i=_assertNodes.begin();i!=_assertNodes.end();++i)
	  if(lab==getLabeler()->getLabel((*i).second))
		labelName=SgNodeHelper::getLabelName((*i).first);
	assert(labelName.size()>0);
	return labelName;
  }

  InputOutput::OpType ioOp(const EState* estate) const;

  void setDisplayDiff(int diff) { _displayDiff=diff; }
  void setSemanticFoldThreshold(int t) { _semanticFoldThreshold=t; }
  void setLTLVerifier(int v) { _ltlVerifier=v; }
  int getLTLVerifier() { return _ltlVerifier; }
  void setNumberOfThreadsToUse(int n) { _numberOfThreadsToUse=n; }
  void insertInputVarValue(int i) { _inputVarValues.insert(i); }
  int numberOfInputVarValues() { return _inputVarValues.size(); }
  list<pair<SgLabelStatement*,SgNode*> > _assertNodes;
  string _csv_assert_live_file; // to become private
  VariableId globalVarIdByName(string varName) { return globalVarName2VarIdMapping[varName]; }

 public:
  // only used temporarily for binary-binding prototype
  map<string,VariableId> globalVarName2VarIdMapping;

 private:
  set<int> _inputVarValues;
  ExprAnalyzer exprAnalyzer;
  VariableIdMapping variableIdMapping;
  EStateWorkList estateWorkList;
  EStateSet estateSet;
  PStateSet pstateSet;
  ConstraintSetMaintainer constraintSetMaintainer;
  TransitionGraph transitionGraph;
  set<const EState*> transitionSourceEStateSetOfLabel(Label lab);
  int _displayDiff;
  int _numberOfThreadsToUse;
  int _ltlVerifier;
  int _semanticFoldThreshold;
};

} // end of namespace CodeThorn

#define RERS_SPECIALIZATION
#ifdef RERS_SPECIALIZATION
// RERS-binary-binding-specific declarations
#define STR_VALUE(arg) #arg
#define COPY_PSTATEVAR_TO_GLOBALVAR(VARNAME) VARNAME = pstate[analyzer->globalVarIdByName(STR_VALUE(VARNAME))].getValue().getIntValue();

//cout<<"PSTATEVAR:"<<pstate[analyzer->globalVarIdByName(STR_VALUE(VARNAME))].toString()<<"="<<pstate[analyzer->globalVarIdByName(STR_VALUE(VARNAME))].getValue().toString()<<endl;

#define COPY_GLOBALVAR_TO_PSTATEVAR(VARNAME) pstate[analyzer->globalVarIdByName(STR_VALUE(VARNAME))]=CodeThorn::AType::CppCapsuleConstIntLattice(VARNAME);

namespace RERS_Problem {
  void rersGlobalVarsCallInit(CodeThorn::Analyzer* analyzer, CodeThorn::PState& pstate);
  void rersGlobalVarsCallReturnInit(CodeThorn::Analyzer* analyzer, CodeThorn::PState& pstate);
  int calculate_output(int);
}
// END OF RERS-binary-binding-specific declarations
#endif

#endif

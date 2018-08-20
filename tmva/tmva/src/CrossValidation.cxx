// @(#)root/tmva $Id$
// Author: Omar Zapata, Thomas James Stevenson and Pourya Vakilipourtakalou
// Modified: Kim Albertsson 2017

/*************************************************************************
 * Copyright (C) 2018, Rene Brun and Fons Rademakers.                    *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

#include "TMVA/CrossValidation.h"

#include "TMVA/ClassifierFactory.h"
#include "TMVA/Config.h"
#include "TMVA/CvSplit.h"
#include "TMVA/DataSet.h"
#include "TMVA/Event.h"
#include "TMVA/MethodBase.h"
#include "TMVA/MethodCrossValidation.h"
#include "TMVA/MsgLogger.h"
#include "TMVA/ResultsClassification.h"
#include "TMVA/ResultsMulticlass.h"
#include "TMVA/ROCCurve.h"
#include "TMVA/tmvaglob.h"
#include "TMVA/Types.h"

#include "TSystem.h"
#include "TAxis.h"
#include "TCanvas.h"
#include "TGraph.h"
#include "TMath.h"

#include <iostream>
#include <memory>

//_______________________________________________________________________
TMVA::CrossValidationResult::CrossValidationResult(UInt_t numFolds)
:fROCCurves(new TMultiGraph())
{
   fSigs.resize(numFolds);
   fSeps.resize(numFolds);
   fEff01s.resize(numFolds);
   fEff10s.resize(numFolds);
   fEff30s.resize(numFolds);
   fEffAreas.resize(numFolds);
   fTrainEff01s.resize(numFolds);
   fTrainEff10s.resize(numFolds);
   fTrainEff30s.resize(numFolds);
}

//_______________________________________________________________________
TMVA::CrossValidationResult::CrossValidationResult(const CrossValidationResult &obj)
{
   fROCs=obj.fROCs;
   fROCCurves = obj.fROCCurves;

   fSigs = obj.fSigs;
   fSeps = obj.fSeps;
   fEff01s = obj.fEff01s;
   fEff10s = obj.fEff10s;
   fEff30s = obj.fEff30s;
   fEffAreas = obj.fEffAreas;
   fTrainEff01s = obj.fTrainEff01s;
   fTrainEff10s = obj.fTrainEff10s;
   fTrainEff30s = obj.fTrainEff30s;
}

//_______________________________________________________________________
void TMVA::CrossValidationResult::Fill(CrossValidationFoldResult const & fr)
{
   UInt_t iFold = fr.fFold;

   fROCs[iFold] = fr.fROCIntegral;
   fROCCurves->Add(static_cast<TGraph *>(fr.fROC.Clone()));

   fSigs[iFold] = fr.fSig;
   fSeps[iFold] = fr.fSep;
   fEff01s[iFold] = fr.fEff01;
   fEff10s[iFold] = fr.fEff10;
   fEff30s[iFold] = fr.fEff30;
   fEffAreas[iFold] = fr.fEffArea;
   fTrainEff01s[iFold] = fr.fTrainEff01;
   fTrainEff10s[iFold] = fr.fTrainEff10;
   fTrainEff30s[iFold] = fr.fTrainEff30;
}

//_______________________________________________________________________
TMultiGraph *TMVA::CrossValidationResult::GetROCCurves(Bool_t /*fLegend*/)
{
   return fROCCurves.get();
}

////////////////////////////////////////////////////////////////////////////////
/// \brief Generates a multigraph that contains an average ROC Curve.
/// \param numSamples[in] Number of Samples to use for generating the average
///                       ROC Curve.
/// \param drawFolds[in]  If true, the multigraph will also contain the individual
///                       ROC Curves of all the folds.
///

//
TMultiGraph *TMVA::CrossValidationResult::GetAvgROCCurve(UInt_t numSamples, Bool_t drawFolds)
{
   TMultiGraph *AvgROCCurve;
   
   if(drawFolds == kFALSE)
     AvgROCCurve = new TMultiGraph();

   else
     AvgROCCurve = fROCCurves.get();

   Double_t interval = 1.0 / numSamples;
   Double_t x[numSamples], y[numSamples];

   Double_t xPoint = 0;
   Double_t rocSum = 0;

   TList *ROCCurveList = fROCCurves.get()->GetListOfGraphs();

   for(UInt_t i=0; i<numSamples; i++){

      for(Int_t j=0; j<ROCCurveList->GetSize(); j++){

        TGraph *foldROC = static_cast<TGraph *>(ROCCurveList->At(j));
        foldROC->SetLineColor(1);
        foldROC->SetLineWidth(1);
        rocSum += foldROC->Eval(xPoint,0,"");
      }

      x[i] = xPoint;
      y[i] = rocSum/ROCCurveList->GetSize();

      rocSum = 0;
      xPoint += interval;
       
   }

   TGraph *AvgROCCurveGraph = new TGraph(numSamples,x,y);
   AvgROCCurveGraph->SetTitle("Avg ROC Curve");
   AvgROCCurveGraph->SetLineColor(2);
   AvgROCCurveGraph->SetLineWidth(3);
   AvgROCCurve->Add(AvgROCCurveGraph);

   return AvgROCCurve;
}

//_______________________________________________________________________
Float_t TMVA::CrossValidationResult::GetROCAverage() const
{
   Float_t avg=0;
   for(auto &roc:fROCs) avg+=roc.second;
   return avg/fROCs.size();
}

//_______________________________________________________________________
Float_t TMVA::CrossValidationResult::GetROCStandardDeviation() const
{
   // NOTE: We are using here the unbiased estimation of the standard deviation.
   Float_t std=0;
   Float_t avg=GetROCAverage();
   for(auto &roc:fROCs) std+=TMath::Power(roc.second-avg, 2);
   return TMath::Sqrt(std/float(fROCs.size()-1.0));
}

//_______________________________________________________________________
void TMVA::CrossValidationResult::Print() const
{
   TMVA::MsgLogger::EnableOutput();
   TMVA::gConfig().SetSilent(kFALSE);

   MsgLogger fLogger("CrossValidation");
   fLogger << kHEADER << " ==== Results ====" << Endl;
   for(auto &item:fROCs)
      fLogger << kINFO << Form("Fold  %i ROC-Int : %.4f",item.first,item.second) << std::endl;

   fLogger << kINFO << "------------------------" << Endl;
   fLogger << kINFO << Form("Average ROC-Int : %.4f",GetROCAverage()) << Endl;
   fLogger << kINFO << Form("Std-Dev ROC-Int : %.4f",GetROCStandardDeviation()) << Endl;

   TMVA::gConfig().SetSilent(kTRUE);
}

//_______________________________________________________________________
TCanvas* TMVA::CrossValidationResult::Draw(const TString name) const
{
   TCanvas *c=new TCanvas(name.Data());
   fROCCurves->Draw("AL");
   fROCCurves->GetXaxis()->SetTitle(" Signal Efficiency ");
   fROCCurves->GetYaxis()->SetTitle(" Background Rejection ");
   Float_t adjust=1+fROCs.size()*0.01;
   c->BuildLegend(0.15,0.15,0.4*adjust,0.5*adjust);
   c->SetTitle("Cross Validation ROC Curves");
   c->Draw();
   return c;
}

//
TCanvas* TMVA::CrossValidationResult::DrawAvgROCCurve(const TString name, Bool_t drawFolds)
{

   TMultiGraph *AvgROCCurve = GetAvgROCCurve(100, drawFolds);
   TCanvas *c=new TCanvas(name.Data());
   AvgROCCurve->Draw("AL");
   AvgROCCurve->GetXaxis()->SetTitle(" Signal Efficiency ");
   AvgROCCurve->GetYaxis()->SetTitle(" Background Rejection ");
   
   TLegend *leg = new TLegend();
   TList *ROCCurveList = AvgROCCurve->GetListOfGraphs();
   
   if(drawFolds == kTRUE){
     
     leg->AddEntry(static_cast<TGraph *>(ROCCurveList->At(0)), "ROC Curves" ,"l");
     leg->AddEntry(static_cast<TGraph *>(ROCCurveList->At(ROCCurveList->GetSize()-1)), "Avg ROC Curve" ,"l");
     leg->Draw();
   }
   else
     c->BuildLegend();
   
   c->SetTitle("Cross Validation Avergare ROC Curve");
   c->Draw();
   return c; 
}

/**
* \class TMVA::CrossValidation
* \ingroup TMVA
* \brief

Use html for explicit line breaking<br>
Markdown links? [class reference](#reference)?


~~~{.cpp}
ce->BookMethod(dataloader, options);
ce->Evaluate();
~~~

Cross-evaluation will generate a new training and a test set dynamically from
from `K` folds. These `K` folds are generated by splitting the input training
set. The input test set is currently ignored.

This means that when you specify your DataSet you should include all events
in your training set. One way of doing this would be the following:

~~~{.cpp}
dataloader->AddTree( signalTree, "cls1" );
dataloader->AddTree( background, "cls2" );
dataloader->PrepareTrainingAndTestTree( "", "", "nTest_cls1=1:nTest_cls2=1" );
~~~

## Split Expression
See CVSplit documentation?

*/

////////////////////////////////////////////////////////////////////////////////
///

TMVA::CrossValidation::CrossValidation(TString jobName, TMVA::DataLoader *dataloader, TFile *outputFile,
                                       TString options)
   : TMVA::Envelope(jobName, dataloader, nullptr, options),
     fAnalysisType(Types::kMaxAnalysisType),
     fAnalysisTypeStr("auto"),
     fCorrelations(kFALSE),
     fCvFactoryOptions(""),
     fDrawProgressBar(kFALSE),
     fFoldFileOutput(kFALSE),
     fFoldStatus(kFALSE),
     fJobName(jobName),
     fNumFolds(2),
     fNumWorkerProcs(1),
     fOutputFactoryOptions(""),
     fOutputFile(outputFile),
     fSilent(kFALSE),
     fSplitExprString(""),
     fROC(kTRUE),
     fTransformations(""),
     fVerbose(kFALSE),
     fVerboseLevel(kINFO)
{
   InitOptions();
   ParseOptions();
   CheckForUnusedOptions();
}

////////////////////////////////////////////////////////////////////////////////
///

TMVA::CrossValidation::CrossValidation(TString jobName, TMVA::DataLoader *dataloader, TString options)
   : CrossValidation(jobName, dataloader, nullptr, options)
{
}

////////////////////////////////////////////////////////////////////////////////
///

TMVA::CrossValidation::~CrossValidation() {}

////////////////////////////////////////////////////////////////////////////////
///

void TMVA::CrossValidation::InitOptions()
{
   // Forwarding of Factory options
   DeclareOptionRef(fSilent, "Silent",
                    "Batch mode: boolean silent flag inhibiting any output from TMVA after the creation of the factory "
                    "class object (default: False)");
   DeclareOptionRef(fVerbose, "V", "Verbose flag");
   DeclareOptionRef(fVerboseLevel = TString("Info"), "VerboseLevel", "VerboseLevel (Debug/Verbose/Info)");
   AddPreDefVal(TString("Debug"));
   AddPreDefVal(TString("Verbose"));
   AddPreDefVal(TString("Info"));

   DeclareOptionRef(fTransformations, "Transformations",
                    "List of transformations to test; formatting example: \"Transformations=I;D;P;U;G,D\", for "
                    "identity, decorrelation, PCA, Uniform and Gaussianisation followed by decorrelation "
                    "transformations");

   DeclareOptionRef(fDrawProgressBar, "DrawProgressBar", "Boolean to show draw progress bar");
   DeclareOptionRef(fCorrelations, "Correlations", "Boolean to show correlation in output");
   DeclareOptionRef(fROC, "ROC", "Boolean to show ROC in output");

   TString analysisType("Auto");
   DeclareOptionRef(fAnalysisTypeStr, "AnalysisType",
                    "Set the analysis type (Classification, Regression, Multiclass, Auto) (default: Auto)");
   AddPreDefVal(TString("Classification"));
   AddPreDefVal(TString("Regression"));
   AddPreDefVal(TString("Multiclass"));
   AddPreDefVal(TString("Auto"));

   // Options specific to CE
   DeclareOptionRef(fSplitExprString, "SplitExpr", "The expression used to assign events to folds");
   DeclareOptionRef(fNumFolds, "NumFolds", "Number of folds to generate");
   DeclareOptionRef(fNumWorkerProcs, "NumWorkerProcs",
      "Determines how many processes to use for evaluation. 1 means no"
      " parallelisation. 2 means use 2 processes. 0 means figure out the"
      " number automatically based on the number of cpus available. Default"
      " 1.");

   DeclareOptionRef(fFoldFileOutput, "FoldFileOutput",
                    "If given a TMVA output file will be generated for each fold. Filename will be the same as "
                    "specifed for the combined output with a _foldX suffix. (default: false)");

   DeclareOptionRef(fOutputEnsembling = TString("None"), "OutputEnsembling",
                    "Combines output from contained methods. If None, no combination is performed. (default None)");
   AddPreDefVal(TString("None"));
   AddPreDefVal(TString("Avg"));
}

////////////////////////////////////////////////////////////////////////////////
///

void TMVA::CrossValidation::ParseOptions()
{
   this->Envelope::ParseOptions();

   // Factory options
   fAnalysisTypeStr.ToLower();
   if (fAnalysisTypeStr == "classification")
      fAnalysisType = Types::kClassification;
   else if (fAnalysisTypeStr == "regression")
      fAnalysisType = Types::kRegression;
   else if (fAnalysisTypeStr == "multiclass")
      fAnalysisType = Types::kMulticlass;
   else if (fAnalysisTypeStr == "auto")
      fAnalysisType = Types::kNoAnalysisType;

   if (fVerbose) {
      fCvFactoryOptions += "V:";
      fOutputFactoryOptions += "V:";
   } else {
      fCvFactoryOptions += "!V:";
      fOutputFactoryOptions += "!V:";
   }

   fCvFactoryOptions += Form("VerboseLevel=%s:", fVerboseLevel.Data());
   fOutputFactoryOptions += Form("VerboseLevel=%s:", fVerboseLevel.Data());

   fCvFactoryOptions += Form("AnalysisType=%s:", fAnalysisTypeStr.Data());
   fOutputFactoryOptions += Form("AnalysisType=%s:", fAnalysisTypeStr.Data());

   if (not fDrawProgressBar) {
      fOutputFactoryOptions += "!DrawProgressBar:";
   }

   if (fTransformations != "") {
      fCvFactoryOptions += Form("Transformations=%s:", fTransformations.Data());
      fOutputFactoryOptions += Form("Transformations=%s:", fTransformations.Data());
   }

   if (fCorrelations) {
      // fCvFactoryOptions += "Correlations:";
      fOutputFactoryOptions += "Correlations:";
   } else {
      // fCvFactoryOptions += "!Correlations:";
      fOutputFactoryOptions += "!Correlations:";
   }

   if (fROC) {
      // fCvFactoryOptions += "ROC:";
      fOutputFactoryOptions += "ROC:";
   } else {
      // fCvFactoryOptions += "!ROC:";
      fOutputFactoryOptions += "!ROC:";
   }

   if (fSilent) {
      // fCvFactoryOptions += Form("Silent:");
      fOutputFactoryOptions += Form("Silent:");
   }

   fCvFactoryOptions += "!Correlations:!ROC:!Color:!DrawProgressBar:Silent";

   // CE specific options
   if (fFoldFileOutput and fOutputFile == nullptr) {
      Log() << kFATAL << "No output file given, cannot generate per fold output." << Endl;
   }

   // Initialisations

   fFoldFactory = std::unique_ptr<TMVA::Factory>(new TMVA::Factory(fJobName, fCvFactoryOptions));

   // The fOutputFactory should always have !ModelPersistence set since we use a custom code path for this.
   //    In this case we create a special method (MethodCrossValidation) that can only be used by
   //    CrossValidation and the Reader.
   if (fOutputFile == nullptr) {
      fFactory = std::unique_ptr<TMVA::Factory>(new TMVA::Factory(fJobName, fOutputFactoryOptions));
   } else {
      fFactory = std::unique_ptr<TMVA::Factory>(new TMVA::Factory(fJobName, fOutputFile, fOutputFactoryOptions));
   }

   fSplit = std::unique_ptr<CvSplitKFolds>(new CvSplitKFolds(fNumFolds, fSplitExprString));
}

//_______________________________________________________________________
void TMVA::CrossValidation::SetNumFolds(UInt_t i)
{
   if (i != fNumFolds) {
      fNumFolds = i;
      fSplit = std::unique_ptr<CvSplitKFolds>(new CvSplitKFolds(fNumFolds, fSplitExprString));
      fDataLoader->MakeKFoldDataSet(*fSplit.get());
      fFoldStatus = kTRUE;
   }
}

////////////////////////////////////////////////////////////////////////////////
///

void TMVA::CrossValidation::SetSplitExpr(TString splitExpr)
{
   if (splitExpr != fSplitExprString) {
      fSplitExprString = splitExpr;
      fSplit = std::unique_ptr<CvSplitKFolds>(new CvSplitKFolds(fNumFolds, fSplitExprString));
      fDataLoader->MakeKFoldDataSet(*fSplit.get());
      fFoldStatus = kTRUE;
   }
}

////////////////////////////////////////////////////////////////////////////////
/// Evaluates each fold in turn.
///   - Prepares train and test data sets
///   - Trains method
///   - Evalutes on test set
///   - Stores the evaluation internally
///
/// @param iFold fold to evaluate
///

TMVA::CrossValidationFoldResult TMVA::CrossValidation::ProcessFold(UInt_t iFold, UInt_t iMethod)
{
   TString methodName = fMethods[iMethod].GetValue<TString>("MethodName");
   TString methodTitle = fMethods[iMethod].GetValue<TString>("MethodTitle");
   TString methodOptions = fMethods[iMethod].GetValue<TString>("MethodOptions");

   Log() << kDEBUG << "Fold (" << methodTitle << "): " << iFold << Endl;

   // Get specific fold of dataset and setup method
   TString foldTitle = methodTitle;
   foldTitle += "_fold";
   foldTitle += iFold + 1;

   // Only used if fFoldOutputFile == true
   TFile *foldOutputFile = nullptr;

   if (fFoldFileOutput and fOutputFile != nullptr) {
      TString path = std::string("") + gSystem->DirName(fOutputFile->GetName()) + "/" + foldTitle + ".root";
      std::cout << "PATH: " << path << std::endl;
      foldOutputFile = TFile::Open(path, "RECREATE");
      fFoldFactory = std::unique_ptr<TMVA::Factory>(new TMVA::Factory(fJobName, foldOutputFile, fCvFactoryOptions));
   }

   fDataLoader->PrepareFoldDataSet(*fSplit.get(), iFold, TMVA::Types::kTraining);
   MethodBase *smethod = fFoldFactory->BookMethod(fDataLoader.get(), methodName, foldTitle, methodOptions);

   // Train method (train method and eval train set)
   Event::SetIsTraining(kTRUE);
   smethod->TrainMethod();
   Event::SetIsTraining(kFALSE);

   fFoldFactory->TestAllMethods();
   fFoldFactory->EvaluateAllMethods();

   TMVA::CrossValidationFoldResult result(iFold);

   // Results for aggregation (ROC integral, efficiencies etc.)
   if (fAnalysisType == Types::kClassification or fAnalysisType == Types::kMulticlass) {
      result.fROCIntegral = fFoldFactory->GetROCIntegral(fDataLoader->GetName(), foldTitle);

      TGraph *gr = fFoldFactory->GetROCCurve(fDataLoader->GetName(), foldTitle, true);
      gr->SetLineColor(iFold + 1);
      gr->SetLineWidth(2);
      gr->SetTitle(foldTitle.Data());
      result.fROC = *gr;

      result.fSig = smethod->GetSignificance();
      result.fSep = smethod->GetSeparation();

      if (fAnalysisType == Types::kClassification) {
         Double_t err;
         result.fEff01 = smethod->GetEfficiency("Efficiency:0.01", Types::kTesting, err);
         result.fEff10 = smethod->GetEfficiency("Efficiency:0.10", Types::kTesting, err);
         result.fEff30 = smethod->GetEfficiency("Efficiency:0.30", Types::kTesting, err);
         result.fEffArea = smethod->GetEfficiency("", Types::kTesting, err);
         result.fTrainEff01 = smethod->GetTrainingEfficiency("Efficiency:0.01");
         result.fTrainEff10 = smethod->GetTrainingEfficiency("Efficiency:0.10");
         result.fTrainEff30 = smethod->GetTrainingEfficiency("Efficiency:0.30");
      } else if (fAnalysisType == Types::kMulticlass) {
         // Nothing here for now
      }
   }

   // Per-fold file output
   if (fFoldFileOutput) {
      foldOutputFile->Close();
   }

   // Clean-up for this fold
   {
      smethod->Data()->DeleteAllResults(Types::kTraining, smethod->GetAnalysisType());
      smethod->Data()->DeleteAllResults(Types::kTesting, smethod->GetAnalysisType());
   }

   fFoldFactory->DeleteAllMethods();
   fFoldFactory->fMethodsMap.clear();

   return result;
}

////////////////////////////////////////////////////////////////////////////////
/// Does training, test set evaluation and performance evaluation of using
/// cross-evalution.
///

void TMVA::CrossValidation::Evaluate()
{
   // Generate K folds on given dataset
   if (!fFoldStatus) {
      fDataLoader->MakeKFoldDataSet(*fSplit.get());
      fFoldStatus = kTRUE;
   }

   fResults.reserve(fMethods.size());
   for (UInt_t iMethod = 0; iMethod < fMethods.size(); iMethod++) {
      CrossValidationResult result{fNumFolds};

      TString methodTypeName = fMethods[iMethod].GetValue<TString>("MethodName");
      TString methodTitle = fMethods[iMethod].GetValue<TString>("MethodTitle");

      if (methodTypeName == "") {
         Log() << kFATAL << "No method booked for cross-validation" << Endl;
      }

      TMVA::MsgLogger::EnableOutput();
      Log() << kINFO << "Evaluate method: " << methodTitle << Endl;

      // Process K folds
      auto nWorkers = fNumWorkerProcs;
      if (nWorkers == 1) {
         // Fall back to global config
         nWorkers = TMVA::gConfig().GetNumWorkers();
      }
      if (nWorkers == 1) {
         for (UInt_t iFold = 0; iFold < fNumFolds; ++iFold) {
            auto fold_result = ProcessFold(iFold, iMethod);
            result.Fill(fold_result);
         }
      } else {
         ROOT::TProcessExecutor workers(nWorkers);
         std::vector<CrossValidationFoldResult> result_vector;

         auto workItem = [this, iMethod](UInt_t iFold) {
            return ProcessFold(iFold, iMethod);
         };

         result_vector = workers.Map(workItem, ROOT::TSeqI(fNumFolds));

         for (auto && fold_result : result_vector) {
            result.Fill(fold_result);
         }
      }

      fResults.push_back(result);

      // Serialise the cross evaluated method
      TString options =
         Form("SplitExpr=%s:NumFolds=%i"
              ":EncapsulatedMethodName=%s"
              ":EncapsulatedMethodTypeName=%s"
              ":OutputEnsembling=%s",
              fSplitExprString.Data(), fNumFolds, methodTitle.Data(), methodTypeName.Data(), fOutputEnsembling.Data());

      fFactory->BookMethod(fDataLoader.get(), Types::kCrossValidation, methodTitle, options);

      // Feed EventToFold mapping used when random fold assignments are used
      // (when splitExpr="").
      IMethod *method_interface = fFactory->GetMethod(fDataLoader.get()->GetName(), methodTitle);
      MethodCrossValidation *method = dynamic_cast<MethodCrossValidation *>(method_interface);

      method->fEventToFoldMapping = fSplit.get()->fEventToFoldMapping;
   }

   // Recombination of data (making sure there is data in training and testing trees).
   fDataLoader->RecombineKFoldDataSet(*fSplit.get());

   // "Eval" on training set
   for (UInt_t iMethod = 0; iMethod < fMethods.size(); iMethod++) {
      TString methodTypeName = fMethods[iMethod].GetValue<TString>("MethodName");
      TString methodTitle = fMethods[iMethod].GetValue<TString>("MethodTitle");

      IMethod *method_interface = fFactory->GetMethod(fDataLoader.get()->GetName(), methodTitle);
      MethodCrossValidation *method = dynamic_cast<MethodCrossValidation *>(method_interface);

      if (fOutputFile) {
         fFactory->WriteDataInformation(method->fDataSetInfo);
      }

      Event::SetIsTraining(kTRUE);
      method->TrainMethod();
      Event::SetIsTraining(kFALSE);
   }

   // Eval on Testing set
   fFactory->TestAllMethods();

   // Calc statistics
   fFactory->EvaluateAllMethods();

   Log() << kINFO << "Evaluation done." << Endl;
}

//_______________________________________________________________________
const std::vector<TMVA::CrossValidationResult> &TMVA::CrossValidation::GetResults() const
{
   if (fResults.size() == 0)
      Log() << kFATAL << "No cross-validation results available" << Endl;
   return fResults;
}

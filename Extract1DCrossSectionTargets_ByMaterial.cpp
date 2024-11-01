//File: ExtractCrossSection.cpp
//Brief: Given data and MC files from analyses/studies/CrossSection.h, extract a 1D differential cross section.
//       Subtracts backgrounds, performs unfolding, applies efficiency x acceptance correction, and 
//       divides by flux and number of nucleons.  Writes a .root file with the cross section histogram.
//
//Usage: ExtractCrossSection <unfolding iterations> <data.root> <mc.root>
//
//Author: Andrew Olivier aolivier@ur.rochester.edu

//util includes
#include "util/GetIngredient.h"

//UnfoldUtils includes
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverloaded-virtual"
#include "MinervaUnfold/MnvUnfold.h"

//PlotUtils includes
#include "PlotUtils/MnvH1D.h"
#include "PlotUtils/MnvH2D.h"
#include "PlotUtils/MnvPlotter.h"
#pragma GCC diagnostic pop

//ROOT includes
#include "TH1D.h"
#include "TFile.h"
#include "TKey.h"
#include "TParameter.h"
#include "TCanvas.h"
#include "string.h"

//Cintex is only needed for older ROOT versions like the GPVMs.
////Let CMake decide whether it's needed.
#ifndef NCINTEX
#include "Cintex/Cintex.h"
#endif

//c++ includes
#include <iostream>
#include <exception>
#include <algorithm>
#include <numeric>

//Convince the STL to talk to TIter so I can use std::find_if()
namespace std
{
  template <>
  struct iterator_traits<TIter>
  {
    using value_type = TObject;
    using pointer = TObject*;
    using reference = TObject&;
    using iterator_category = forward_iterator_tag;
  };
}

//Plot a step in cross section extraction.
void Plot(PlotUtils::MnvH1D& hist, const std::string& stepName, const std::string& prefix)
{
  TCanvas can(stepName.c_str());
  hist.GetCVHistoWithError().Clone()->Draw();
  can.Print((prefix + "_" + stepName + ".png").c_str());

  //Uncertainty summary
  PlotUtils::MnvPlotter plotter;
  plotter.ApplyStyle(PlotUtils::kCCQENuStyle);
  plotter.axis_maximum = 0.4;

  plotter.DrawErrorSummary(&hist);
  can.Print((prefix + "_" + stepName + "_uncertaintySummary.png").c_str());

  plotter.DrawErrorSummary(&hist, "TR", true, true, 1e-5, false, "Other");
  can.Print((prefix + "_" + stepName + "_otherUncertainties.png").c_str());
}

//Unfolding function from Aaron Bercelle
//TODO: Trim it down a little?  Remove that static?
PlotUtils::MnvH1D* UnfoldHist( PlotUtils::MnvH1D* h_folded, PlotUtils::MnvH2D* h_migration, int num_iter )
{
  static MinervaUnfold::MnvUnfold unfold;
  PlotUtils::MnvH1D* h_unfolded = nullptr;

  //bool bUnfolded = false;

  TMatrixD dummyCovMatrix;
  if(!unfold.UnfoldHisto( h_unfolded, dummyCovMatrix, h_migration, h_folded, RooUnfold::kBayes, num_iter, true, false ))
    return nullptr;

  /////////////////////////////////////////////////////////////////////////////////////////  
  //No idea if this is still needed
  //Probably.  This gets your stat unfolding covariance matrix
  TMatrixD unfoldingCovMatrixOrig; 
  int correctNbins;
  int matrixRows;  
  TH1D* hUnfoldedDummy  = new TH1D(h_unfolded->GetCVHistoWithStatError());
  TH1D* hRecoDummy      = new TH1D(h_migration->ProjectionX()->GetCVHistoWithStatError());
  TH1D* hTruthDummy     = new TH1D(h_migration->ProjectionY()->GetCVHistoWithStatError());
  TH1D* hBGSubDataDummy = new TH1D(h_folded->GetCVHistoWithStatError());
  TH2D* hMigrationDummy = new TH2D(h_migration->GetCVHistoWithStatError());
  unfold.UnfoldHisto(hUnfoldedDummy, unfoldingCovMatrixOrig, hMigrationDummy, hRecoDummy, hTruthDummy, hBGSubDataDummy,RooUnfold::kBayes, num_iter);//Stupid RooUnfold.  This is dummy, we don't need iterations

  correctNbins=hUnfoldedDummy->fN;
  matrixRows=unfoldingCovMatrixOrig.GetNrows();
  if(correctNbins!=matrixRows){
    std::cout << "****************************************************************************" << std::endl;
    std::cout << "*  Fixing unfolding matrix size because of RooUnfold bug. From " << matrixRows << " to " << correctNbins << std::endl;
    std::cout << "****************************************************************************" << std::endl;
    // It looks like this, since the extra last two bins don't have any content
    unfoldingCovMatrixOrig.ResizeTo(correctNbins, correctNbins);
  }

  for(int i=0; i<unfoldingCovMatrixOrig.GetNrows(); ++i) unfoldingCovMatrixOrig(i,i)=0;
  delete hUnfoldedDummy;
  delete hMigrationDummy;
  delete hRecoDummy;
  delete hTruthDummy;
  delete hBGSubDataDummy;
  h_unfolded->PushCovMatrix("unfoldingCov",unfoldingCovMatrixOrig);

  /////////////////////////////////////////////////////////////////////////////////////////  
  return h_unfolded;
}

//The final step of cross section extraction: normalize by flux, bin width, POT, and number of targets
PlotUtils::MnvH1D* normalize(PlotUtils::MnvH1D* efficiencyCorrected, PlotUtils::MnvH1D* fluxIntegral, const double nNucleons, const double POT)
{
  efficiencyCorrected->Divide(efficiencyCorrected, fluxIntegral);

  efficiencyCorrected->Scale(1./nNucleons/POT);
  efficiencyCorrected->Scale(1.e4); //Flux histogram is in m^-2, but convention is to report cm^2
  efficiencyCorrected->Scale(1., "width");

  return efficiencyCorrected;
}

int main(const int argc, const char** argv)
{
  #ifndef NCINTEX
  ROOT::Cintex::Cintex::Enable(); //Needed to look up dictionaries for PlotUtils classes like MnvH1D
  #endif

  TH1::AddDirectory(kFALSE); //Needed so that MnvH1D gets to clean up its own MnvLatErrorBands (which are TH1Ds).

  if(argc != 4 && argc != 5 )
  {
    std::cerr << "Expected 3 or 4 arguments, but I got " << argc-1 << ".\n"
              << "USAGE: ExtractCrossSection <unfolding iterations> <data.root> <mc.root> <OPTIONAL:numPlaylists>\n"
              << "Where <numPlaylists> is the number of playlists merged to make <data.root> and <mc.root>. If empty, it will be assumed to be 1.\n";
    return 1;
  }
  int numMergedPlaylists = 1;

  const int nIterations = std::stoi(argv[1]);
  auto dataFile = TFile::Open(argv[2], "READ");
  if(!dataFile)
  {
    std::cerr << "Failed to open data file " << argv[2] << ".\n";
    return 2;
  }

  auto mcFile = TFile::Open(argv[3], "READ");
  if(!mcFile)
  {
    std::cerr << "Failed to open MC file " << argv[3] << ".\n";
    return 3;
  }

  if (argc == 5)
  {
    numMergedPlaylists = std::stoi(argv[4]);
  }

  std::vector<std::string> crossSectionPrefixes = {"nuke_pTmu", "nuke_pZmu", "nuke_BjorkenX", "nuke_Erecoil", "nuke_Emu"};

  std::map<std::string, std::vector <std::string>> MatsToTargets; //mapping which targets constitute which materials

  MatsToTargets.insert({"PseudoTargets", {"Target7", "Target8", "Target9", "Target10", "Target11", "Target12"}});
  MatsToTargets.insert({"Iron", {"1026", "2026", "3026"}});
  MatsToTargets.insert({"Lead", {"1082", "2082", "3082", "4082", "5082"}});
  MatsToTargets.insert({"Carbon", {"3006"}});
  MatsToTargets.insert({"Water", {"3006"}});

  std::vector<std::string> targets = {"Target7", "Target8", "Target9", "Target10", "Target11", "Target12", "1026", "1082", "2026", "2082", "3006", "3026", "3082", "4082", "5026", "5082", "Water"}; //Is there any benefit to getting this programatically like above for the 1D prefixes?


  /* for(auto key: *dataFile->GetListOfKeys())
  {
    const std::string keyName = key->GetName();
    if (keyName == "POTUsed") continue;
    std::cout << "keyName " << keyName <<std::endl;
    const size_t endOfPrefix = keyName.find("_data");
    std::string prefix = keyName.substr(0, endOfPrefix);
    std::cout << "prefix " << prefix <<std::endl;
    //This counts the number of '_' in the prefix, which should match the dimension, eg 2D would be nuke_pTmu_pZmu, which has 2 underscores
    bool twoDimension = (keyName == "_data_nuke_pTmu_pZmu");

    bool alreadyInVector = std::find(crossSectionPrefixes.begin(), crossSectionPrefixes.end(), prefix) != crossSectionPrefixes.end();
    std::cout << "twoDimension " << twoDimension <<std::endl;
    std::cout << "alreadyInVector " << alreadyInVector <<std::endl;
    if(endOfPrefix != std::string::npos && !alreadyInVector && !twoDimension) crossSectionPrefixes.push_back(prefix);
  } */
  const double mcPOT = util::GetIngredient<TParameter<double>>(*mcFile, "POTUsed")->GetVal(),
               dataPOT = util::GetIngredient<TParameter<double>>(*dataFile, "POTUsed")->GetVal();
  std::cout<<"Data POT: " << dataPOT << " mcPOT " << mcPOT << std::endl;
  for(const auto& prefix: crossSectionPrefixes)
  {
    std::cout<< "Current working on prefix: " << prefix << std::endl;
    for (auto const& mat : MatsToTargets)
    {
      std::string material = mat.first;
      std::vector<std::string> targets = mat.second;
      try
      {
        PlotUtils::MnvH1D *flux, *folded, *effNum, *effDenom;
        PlotUtils::MnvH2D* migration;
        std::vector<PlotUtils::MnvH1D*> backgrounds;
        double nNucleonsVal = 0;
        std::cout<< "Current working on material: " << material << std::endl;

        for (int c = 0; c < targets.size(); c++)
        {
          std::string tgt = targets[c];
          if (c == 0)
          {
            flux = util::GetIngredient<PlotUtils::MnvH1D>(*mcFile, (tgt+std::string("_reweightedflux_integrated")), prefix);
            folded = util::GetIngredient<PlotUtils::MnvH1D>(*dataFile, (std::string("by_TargetCode_Data_")+tgt), prefix);
            migration = util::GetIngredient<PlotUtils::MnvH2D>(*mcFile, (std::string("migration_")+tgt), prefix);
            effNum = util::GetIngredient<PlotUtils::MnvH1D>(*mcFile, (std::string("efficiency_numerator_")+tgt), prefix);
            effDenom = util::GetIngredient<PlotUtils::MnvH1D>(*mcFile, (std::string("efficiency_denominator_")+tgt), prefix);
          }
          else
          {
            flux->Add(util::GetIngredient<PlotUtils::MnvH1D>(*mcFile, (tgt+std::string("_reweightedflux_integrated")), prefix));
            folded->Add(util::GetIngredient<PlotUtils::MnvH1D>(*dataFile, (std::string("by_TargetCode_Data_")+tgt), prefix));
            migration->Add(util::GetIngredient<PlotUtils::MnvH2D>(*mcFile, (std::string("migration_")+tgt), prefix));
            effNum->Add(util::GetIngredient<PlotUtils::MnvH1D>(*mcFile, (std::string("efficiency_numerator_")+tgt), prefix));
            effDenom->Add(util::GetIngredient<PlotUtils::MnvH1D>(*mcFile, (std::string("efficiency_denominator_")+tgt), prefix));
          }
          auto nNucleons = util::GetIngredient<TParameter<double>>(*mcFile, (tgt+std::string("_fiducial_nucleons")), prefix); //Dan: Use the same truth fiducial volume for all extractions.  The acceptance correction corrects data back to this fiducial even if the reco fiducial cut is different.
          nNucleonsVal += nNucleons->GetVal();
          backgrounds.push_back(util::GetIngredient<PlotUtils::MnvH1D>(*mcFile, tgt+std::string("_Wrong_Material_Bkg"), prefix));
          backgrounds.push_back(util::GetIngredient<PlotUtils::MnvH1D>(*mcFile, tgt+std::string("_Wrong_Sign_Bkg"), prefix));
          backgrounds.push_back(util::GetIngredient<PlotUtils::MnvH1D>(*mcFile, tgt+std::string("_NC_Bkg"), prefix));
        }
        flux->Scale(1.0/numMergedPlaylists); //Only possible if the integrated flux is constant between playlists 


        auto simEventRate = effDenom->Clone(); //Make a copy for later
        //There are no error bands in the data, but I need somewhere to put error bands on the results I derive from it.
        folded->AddMissingErrorBandsAndFillWithCV(*migration);

        //Basing my unfolding procedure for a differential cross section on Alex's MINERvA 101 talk at https://minerva-docdb.fnal.gov/cgi-bin/private/RetrieveFile?docid=27438&filename=whatsACrossSection.pdf&version=1

        //TODO: Remove these debugging plots when done
        auto toSubtract = std::accumulate(std::next(backgrounds.begin()), backgrounds.end(), (*backgrounds.begin())->Clone(),
                                          [](auto sum, const auto hist)
                                          {
                                            sum->Add(hist);
                                            return sum;
                                          });
        //Plot(*toSubtract, "BackgroundSum", prefix);

        auto bkgSubtracted = std::accumulate(backgrounds.begin(), backgrounds.end(), folded->Clone(),
                                            [mcPOT, dataPOT](auto sum, const auto hist)
                                            {
                                              std::cout << "Subtracting " << hist->GetName() << " scaled by " << -dataPOT/mcPOT << " from " << sum->GetName() << "\n";
                                              sum->Add(hist, -dataPOT/mcPOT);
                                              return sum;
                                            });
        //Plot(*bkgSubtracted, "backgroundSubtracted", prefix);

        auto outFile = TFile::Open((material+"_"+prefix + "_crossSection.root").c_str(), "CREATE");
        if(!outFile)
        {
          std::cerr << "Could not create a file called " << prefix + "_crossSection.root" << ".  Does it already exist?\n";
          return 5;
        }

        bkgSubtracted->Write("backgroundSubtracted");

        //d'Aogstini unfolding
        auto unfolded = UnfoldHist(bkgSubtracted, migration, nIterations);
        if(!unfolded) throw std::runtime_error(std::string("Failed to unfold ") + folded->GetName() + " using " + migration->GetName());
        //Plot(*unfolded, "unfolded", prefix);
        unfolded->Clone()->Write("unfolded"); //TODO: Seg fault first appears when I uncomment this line
        std::cout << "Survived writing the unfolded histogram.\n" << std::flush; //This is evidence that the problem is on the final file Write() and not unfolded->Clone()->Write().

        effNum->Divide(effNum, effDenom); //Only the 2 parameter version of MnvH1D::Divide()
                                          //handles systematics correctly.
        //Plot(*effNum, "efficiency", prefix);

        unfolded->Divide(unfolded, effNum);
        //Plot(*unfolded, "efficiencyCorrected", prefix);

        auto crossSection = normalize(unfolded, flux, nNucleonsVal/numMergedPlaylists, dataPOT);
        //Plot(*crossSection, "crossSection", prefix);
        crossSection->Clone()->Write("crossSection");
        simEventRate->Write("simulatedEventRate");
        flux->Write("flux_reweighted");
        //Write a "simulated cross section" to compare to the data I just extracted.
        //If this analysis passed its closure test, this should be the same cross section as
        //what GENIEXSecExtract would produce.
        normalize(simEventRate, flux, nNucleonsVal/numMergedPlaylists, mcPOT);
        
        //Plot(*simEventRate, "simulatedCrossSection", prefix);
        simEventRate->Write("simulatedCrossSection");
        outFile->Close();

      }
      catch(const std::runtime_error& e)
      {
        std::cerr << "Failed to extract a cross section for material " << material <<" and prefix " << prefix << ": " << e.what() << "\n";
        return 4;
      }













    }
  }
  dataFile->Close();
  mcFile->Close();
  return 0;
}
#if !defined(__CINT__) || defined(__MAKECINT__)
#include "TString.h"
#include "AliCDBManager.h"
#include "AliCDBEntry.h"
#include "TSystem.h"
#include "AliTriggerConfiguration.h"
#include "AliTriggerBCMask.h"
#include "AliLumiTools.h"
#endif

TString GetMaskMu(int run, double& mu, double xsecBarn = 8.0)
{
  AliCDBManager* man = AliCDBManager::Instance();
  if (gSystem->AccessPathName("OCDB.root", kFileExists)==0) {     
    man->SetDefaultStorage("local://");
    man->SetRaw(kFALSE);
    man->SetSnapshotMode("OCDB.root");
  }
  else if (!man->IsDefaultStorageSet()) {
    man->SetDefaultStorage("raw://");
  }
  man->SetRun(run);
  AliCDBEntry* entry = man->Get("GRP/CTP/Config");
  const AliTriggerConfiguration* cf = (const AliTriggerConfiguration*)entry->GetObject();
  const AliTriggerBCMask* mask = (const AliTriggerBCMask*)cf->GetMasks()[0];
  TString maskStr = mask->GetTitle();
  int nbc = mask->GetNUnmaskedBCs();
  double lumi = AliLumiTools::GetLumiFromCTP(run)->GetMean(2);
  mu = lumi*xsecBarn / 11245.;
  return maskStr;
}

void  AddNuclei(AliGenCocktail *ctl);
AliGenerator* PerformanceGenerator();


AliGenerator * GeneratorCustom() {
  Int_t nrun = atoi(gSystem->Getenv("CONFIG_RUN"));
  printf("Configuration for run %d\n",nrun);
  Double_t mu=0.001; 
  TString bcm=GetMaskMu(nrun,mu);
  // swap H and L to match requirement of AliGenPileup
  bcm.ReplaceAll("H","X");
  bcm.ReplaceAll("L","H");
  bcm.ReplaceAll("X","L");
  //printf("   BC mask = %s\n",bcm.Data());
  printf("   mu = %f\n",mu);
  printf("   Energy = %f\n",energyConfig);
  
  // Set the pileup interaction generator
  // The second argument is the pileup rate
  // in terms of event rate per bunch crossing
  AliGenPileup *genpil = new AliGenPileup();

  AliGenCocktail *ctl    = (AliGenCocktail*)GeneratorCocktail("Hijing+Generator for performance (tracking,PID) studies with pileup");
  AliGenHijing *hijing = (AliGenHijing*)GeneratorHijing();
  AliGenerator   *genJet = (AliGenerator*)PerformanceGenerator();
  ctl->AddGenerator(hijing,    "Hijing", 1.);
  AddNuclei(ctl);
  ctl->AddGenerator(genJet,"Generator for performance (tracking,PID) studies",1);

  genpil->SetGenerator(ctl,mu);
  // Set the beam time structure
  // Details on the syntax in STEER/AliTriggerBCMask
  genpil->SetBCMask(bcm.Data());
  // Generate the trigger interaction
  genpil->GenerateTrigInteraction(kTRUE);
  genpil->Print();
  
  return genpil;
}


/// Make performance generator with flat 1/pt and flat pdg () input jets
/// Jet multiplicity smeared (Poisson) with mean value nJets
/// * nJets  defaults
///   * 1  for pp
///   * 2  for pPb
///   * 10 for PbPb
/// * Number of jets can be overwritten by env variable PerformanceGenerator_nJets
/// \return Performance Generator
AliGenerator* PerformanceGenerator() {
  Printf("======= GeneratorCustom ======");
  gSystem->Load("libEVGEN");
  Int_t nJets =1;
  if (systemConfig.EqualTo("Pb-Pb")) {
    nJets=10;
  }
  if (systemConfig.EqualTo("p-Pb") || systemConfig.EqualTo("Pb-p") ) {
    nJets=2;
  }
  // default value for injected fraction of jets
  if (gSystem->Getenv("PerformanceGenerator_nJets")) nJets= atoi(gSystem->Getenv("PerformanceGenerator_nJets"));
  //
  AliGenPerformance *genPerformance= new AliGenPerformance("AliGenPerformance", AliGenPerformance::kStream| AliGenPerformance::kStreamEvent); //
  genPerformance->SetNJets(nJets);
  printf("AliGenPerformance:   Mean number of jets\t%d\n",nJets);
  TF1* f1pt = new TF1("f1pt","1-10*x",0.0003,0.1);             //
  TF1* fPDG = new TF1("f1pt","x",1,6);                         // flat pdg distribution
  genPerformance->SetFunctions(f1pt,0,0,0,fPDG);
  return genPerformance;
}


/// add Nuclei into performance cocktail (Deuterium, Triton, He3,He4)
/// Mean number of added particles 5 x 8 for PbPb and 0.5x8 for p-p
/// Using AliGenFunction
///     * AliGenFunction to be checked (position)

void AddNuclei(AliGenCocktail *ctl) {
  Int_t kHe3 = 1000020030;
  Int_t kHe4 = 1000020040;
  Int_t kDeuteron = 1000010020;
  Int_t kTriton = 1000010030;
  Int_t pdgCode[8] = {-kDeuteron, kDeuteron, -kTriton, kTriton, -kHe3, kHe3, -kHe4, kHe4};
  Int_t nPart =1;
  if (systemConfig.EqualTo("Pb-Pb")) {
    nPart=5;
  }
  if (systemConfig.EqualTo("p-Pb") || systemConfig.EqualTo("Pb-p") ) {
    nPart=1;
  }
  for (Int_t iPart = 0; iPart < 8; iPart++) {
    AliGenFunction *generPerformance = new AliGenFunction;
    generPerformance->SetNumberParticles(nPart);
    TF1* f1pt = new TF1("f1pt","1/x",0.5,20);  /// flat 1/pt form 0.5-20 GeV/c             //
    f1pt->SetNpx(1000);
    TF1* fPDG = new TF1("fpdg","1",pdgCode[iPart]-0.5,pdgCode[iPart]+0.5);                         // mean number of nuclei injected
    TF1 *fphi   = new TF1("fphi","1",-3.14,3.14);
    TF1 *ftheta = new TF1("ftheta","TMath::Gaus(x,TMath::Pi()/2,0.3)",-3.14,3.14);
    TF3 *fpos = new TF3("fpos","1+(x+y+z)*0",-0.02,0.02,-0.02,0.02,-5,5);
    generPerformance->SetFunctions(f1pt,fphi,ftheta,fpos,fPDG);
    generPerformance->SetCylinder(100,-1,1);
    generPerformance->SetBkG(0.2);
    ctl->AddGenerator(generPerformance,TString::Format("Generator for performance (tracking,PID) studies %d",iPart).Data(),1);
  }
}

/*
 *  Class:PostProcessor 
 *
 *
 *  $Date: 2008/11/05 17:28:44 $
 *  $Revision: 1.8 $
 * 
 *  \author Junghwan Goh - SungKyunKwan University
 */

#include "Validation/RecoMuon/src/PostProcessor.h"

#include "Validation/Tools/interface/FitSlicesYTool.h"
#include "DQMServices/Core/interface/DQMStore.h"
#include "DQMServices/Core/interface/MonitorElement.h"
#include "FWCore/ServiceRegistry/interface/Service.h"

#include <TH1F.h>
#include <cmath>

using namespace std;
using namespace edm;

typedef MonitorElement ME;
typedef vector<string> vstring;

PostProcessor::PostProcessor(const ParameterSet& pset)
{
  verbose_ = pset.getUntrackedParameter<unsigned int>("verbose", 0);

  effCmds_ = pset.getParameter<vstring>("efficiency");
  resCmds_ = pset.getParameter<vstring>("resolution");

  outputFileName_ = pset.getUntrackedParameter<string>("outputFileName", "");
  subDir_ = pset.getUntrackedParameter<string>("subDir");
}

void PostProcessor::endJob()
{
  theDQM = 0;
  theDQM = Service<DQMStore>().operator->();

  if ( ! theDQM ) {
    LogInfo("PostProcessor") << "Cannot create DQMStore instance\n";
    return;
  }

  if ( subDir_[subDir_.size()-1] == '/' ) subDir_.erase(subDir_.size()-1);

  // Process wildcard in the sub-directory
  vector<string> subDirs;
  if ( subDir_[subDir_.size()-1] == '*' ) {
    const string::size_type shiftPos = subDir_.rfind('/');

    const string searchPath = subDir_.substr(0, shiftPos);
    theDQM->cd(searchPath);

    vector<string> foundDirs = theDQM->getSubdirs();
    const string matchStr = subDir_.substr(0, subDir_.size()-2);

    for(vector<string>::const_iterator iDir = foundDirs.begin();
        iDir != foundDirs.end(); ++iDir) {
      const string dirPrefix = iDir->substr(0, matchStr.size());

      if ( dirPrefix == matchStr ) {
        subDirs.push_back(*iDir);
      }
    }
  }
  else {
    subDirs.push_back(subDir_);
  }

  for(vector<string>::const_iterator iSubDir = subDirs.begin();
      iSubDir != subDirs.end(); ++iSubDir) {
    typedef boost::escaped_list_separator<char> elsc;

    const string& dirName = *iSubDir;

    for(vstring::const_iterator iCmd = effCmds_.begin();
        iCmd != effCmds_.end(); ++iCmd) {
      boost::tokenizer<elsc> tokens(*iCmd, elsc("\\", " \t", "\'"));

      vector<string> args;
      for(boost::tokenizer<elsc>::const_iterator iToken = tokens.begin();
          iToken != tokens.end(); ++iToken) {
        if ( iToken->empty() ) continue;
        args.push_back(*iToken);
      }

      if ( args.size() != 4 ) {
        cout << "Wrong input to effCmds\n";
        continue;
      }

      computeEfficiency(dirName, args[0], args[1], args[2], args[3]);
    }

    for(vstring::const_iterator iCmd = resCmds_.begin();
        iCmd != resCmds_.end(); ++ iCmd) {
      boost::tokenizer<elsc> tokens(*iCmd, elsc("\\", " \t", "\'"));

      vector<string> args;
      for(boost::tokenizer<elsc>::const_iterator iToken = tokens.begin();
          iToken != tokens.end(); ++iToken) {
        if ( iToken->empty() ) continue;
        args.push_back(*iToken);
      }

      if ( args.size() != 3 ) {
        cout << "Wrong input to resCmds\n";
        continue;
      }

      computeResolution(dirName, args[0], args[1], args[2]);
    }
  }

  if ( verbose_ > 0 ) theDQM->showDirStructure();

  if ( ! outputFileName_.empty() ) theDQM->save(outputFileName_);
}

void PostProcessor::computeEfficiency(const string& startDir, const string& efficMEName, const string& efficMETitle,
                                      const string& recoMEName, const string& simMEName)
{
  if ( ! theDQM->dirExists(startDir) ) {
    LogError("PostProcessor") << "computeEfficiency() : Cannot find sub-directory " << startDir << endl; 
    return;
  }

  theDQM->cd();

  ME* simME  = theDQM->get(startDir+"/"+simMEName);
  ME* recoME = theDQM->get(startDir+"/"+recoMEName);

  if ( !simME ) {
    LogError("PostProcessor") << "computeEfficiency() : "
                              << "No sim-ME '" << simMEName << "' found\n";
    return;
  }

  if ( !recoME ) {
    LogError("PostProcessor") << "computeEfficiency() : " 
                              << "No reco-ME '" << recoMEName << "' found\n";
    return;
  }

  TH1F* hSim  = simME ->getTH1F();
  TH1F* hReco = recoME->getTH1F();
  if ( !hSim || !hReco ) {
    LogError("PostProcessor") << "computeEfficiency() : Cannot create TH1F from ME\n";
    return;
  }

  string efficDir = startDir;
  string newEfficMEName = efficMEName;
  string::size_type shiftPos;
  if ( string::npos != (shiftPos = efficMEName.rfind('/')) ) {
    efficDir += "/"+efficMEName.substr(0, shiftPos);
    newEfficMEName.erase(0, shiftPos+1);
  }
  theDQM->setCurrentFolder(efficDir);
  ME* efficME = theDQM->book1D(newEfficMEName, efficMETitle, hSim->GetNbinsX(), hSim->GetXaxis()->GetXmin(), hSim->GetXaxis()->GetXmax()); 

  if ( !efficME ) {
    LogError("PostProcessor") << "computeEfficiency() : Cannot book effic-ME from the DQM\n";
    return;
  }

  const int nBin = efficME->getNbinsX();
  for(int bin = 0; bin <= nBin; ++bin) {
    const float nSim  = simME ->getBinContent(bin);
    const float nReco = recoME->getBinContent(bin);
    const float eff = nSim ? nReco/nSim : 0.;
    const float err = nSim && eff <= 1 ? sqrt(eff*(1-eff)/nSim) : 0.;
    efficME->setBinContent(bin, eff);
    efficME->setBinError(bin, err);
  }
  efficME->setEntries(simME->getEntries());

  // Global efficiency
  ME* efficME_value = theDQM->bookFloat(newEfficMEName+"_value");
  ME* efficME_error = theDQM->bookFloat(newEfficMEName+"_error");

  const float nSimAll = hSim->GetEntries();
  const float nRecoAll = hReco->GetEntries();
  const float efficAll = nSimAll ? nRecoAll/nSimAll : 0;
  const float errorAll = nSimAll && efficAll < 1 ? sqrt(efficAll*(1-efficAll)/nSimAll) : 0;
  efficME_value->Fill(efficAll);
  efficME_error->Fill(errorAll);
}

void PostProcessor::computeResolution(const string& startDir, const string& namePrefix, const string& titlePrefix,
                                      const std::string& srcName)
{
  if ( ! theDQM->dirExists(startDir) ) {
    LogError("PostProcessor") << "computeResolution() : Cannot find sub-directory " << startDir << endl;
    return;
  }

  theDQM->cd();

  ME* srcME = theDQM->get(startDir+"/"+srcName);
  if ( !srcME ) {
    LogError("PostProcessor") << "computeResolution() : No source ME '" << srcName << "' found\n";
    return;
  }

  TH2F* hSrc = srcME->getTH2F();
  if ( !hSrc ) {
    LogError("PostProcessor") << "computeResolution() : Cannot create TH2F from source-ME\n";
    return;
  }

  const int nBin = hSrc->GetNbinsX();
  const double xMin = hSrc->GetXaxis()->GetXmin();
  const double xMax = hSrc->GetXaxis()->GetXmax();

  string newDir = startDir;
  string newPrefix = namePrefix;
  string::size_type shiftPos;
  if ( string::npos != (shiftPos = namePrefix.rfind('/')) ) {
    newDir += "/"+namePrefix.substr(0, shiftPos);
    newPrefix.erase(0, shiftPos+1);
  }

  theDQM->setCurrentFolder(newDir);

  ME* meanME = theDQM->book1D(newPrefix+"_Mean", titlePrefix+" Mean", nBin, xMin, xMax);
  ME* sigmaME = theDQM->book1D(newPrefix+"_Sigma", titlePrefix+" Sigma", nBin, xMin, xMax);
//  ME* chi2ME  = theDQM->book1D(namePrefix+"_Chi2" , titlePrefix+" #Chi^{2}", nBin, xMin, xMax); // N/A

  FitSlicesYTool fitTool(srcME);
  fitTool.getFittedMeanWithError(meanME);
  fitTool.getFittedSigmaWithError(sigmaME);
//  fitTool.getFittedChisqWithError(chi2ME); // N/A

}

/* vim:set ts=2 sts=2 sw=2 expandtab: */

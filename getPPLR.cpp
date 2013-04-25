#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <vector>

using namespace std;

#include "ArgumentParser.h"
#include "common.h"
#include "misc.h"
#include "PosteriorSamples.h"

int main(int argc,char* argv[]){
   string programDescription=
"Computes PPLR from MCMC expression samples.\n"
"   (the probability of second condition being up-regulated)\n"
"   Also computes log2 fold change with confidence intervals, and condition mean log expression.\n"
"   [sampleFiles] should contain transposed MCMC samples from different conditions.";
   // Set options {{{
   ArgumentParser args(programDescription,"[sampleFile-C1] [sampleFile-C1]",1);
   args.addOptionS("o","outFile","outFileName",1,"Name of the output file.");
   args.addOptionB("","inputIsLogged","logged",0,"Indicate that the input expression estimates are on log scale. (Not necessary to use with data generated by BitSeq-0.5.0 and above.)");
   args.addOptionB("d","distribution","distribution",0,"Produce whole distribution of differences.");
   args.addOptionS("s","selectFile","selectFileName",0,"File containing list of selected transcript IDs (zero based), only these will be reported. Only works with --distribution option.");
   args.addOptionD("","subSample","subSample",0,"Sub-sample the distributions using a given fraction of expression samples.",1.0);
   if(!args.parse(argc,argv))return 0;
   if(args.verbose)buildTime(argv[0],__DATE__,__TIME__);
   // }}}

   long i,m,N,M;
   bool getAll=false, doLog = true;
   vector<long> trSelect;
   if(! args.isSet("selectFileName")){
      getAll=true;
   }else{
      ifstream selectF (args.getS("selectFileName").c_str());
      if(! selectF.is_open()){
         cerr<<"ERROR: Main: Failed loading selected transcripts."<<endl;
         return 1;
      }
      selectF>>m;
      while(selectF.good()){
         trSelect.push_back(m);
         selectF>>m;
      }
      selectF.close();
      sort(trSelect.begin(),trSelect.end());
   }

   Conditions cond;
   if(! cond.init("NONE", args.args(), &M, &N)){
      cerr<<"ERROR: Main: Failed loading conditions."<<endl;
      return 1;
   }
   if(cond.logged() || args.flag("logged")) {
      doLog = false;
      if(args.verbose)cout<<"Assuming values are logged already."<<endl;
   }else {
      doLog = true;
      if(args.verbose)cout<<"Will use logged values."<<endl;
   }
   if(args.verbose)cout<<"M "<<M<<"   N "<<N<<endl;
   ofstream outFile(args.getS("outFileName").c_str());
   if(! outFile.is_open()){
      cerr<<"ERROR: Main: File write probably failed!"<<endl;
      return 1;
   }
   if(getAll){
      trSelect.resize(M);
      for(i=0;i<M;i++)trSelect[i]=i;
   }
   
   vector<vector<double> > tr(2);
   vector<double> difs;
   long subN = N;
   double frac = args.getD("subSample");
   if((frac > 0) && (frac < 1))subN = (long)(N * frac);
   if(subN<1){
      cerr<<"ERROR: The fraction of samples for sub-sampling is too small."<<endl;
      return 1;
   }
   if((args.getD("subSample")!=1) && args.verbose){
      cout<<"Using "<<subN<<" samples for sub-sampling."<<endl;
   }
   double pplr,mu_0,mu_1,log2FC,ciLow,ciHigh;
   if(! args.flag("distribution")){
      if(args.verbose)cout<<"Counting PPLR"<<endl;
      outFile<<"# Computed PPLR, log2 fold change with 95\% confidence intervals, condition mean log expression."<<endl;
      outFile<<"# M "<<M<<"\n# columns:"<<endl;
      outFile<<"# PPLR log2FoldChange ConfidenceLow ConfidenceHigh MeanLogExpressionC1 MeanLogExpressionC2"<<endl;
      for(m=0;m<M;m++){
         if(args.verbose)progressLog(m,M);
         cond.getTranscript(0,m,tr[0],subN);
         cond.getTranscript(1,m,tr[1],subN);
         difs.resize(subN);
         pplr = log2FC = mu_0 = mu_1 = 0;
         for(i=0;i<subN;i++){
            if(doLog){
               if((tr[0][i] <= 0) || (tr[1][i] <= 0)){
                  cerr<<"ERROR: Found non-positive expression (transcript: "<<m<<").\n"
                        "       The expression is probably in log scale already.\n"
                        "       Please check your data and use --inputIsLogged if that is the case."
                      <<endl;
                  return 1;
               }
               tr[1][i] = log(tr[1][i]);
               tr[0][i] = log(tr[0][i]);
            }
            if(tr[1][i]>tr[0][i])pplr+=1;
            difs[i]=tr[1][i]-tr[0][i];
            log2FC+=tr[1][i]-tr[0][i];
            mu_0 += tr[0][i];
            mu_1 += tr[1][i];
         }
         pplr /= subN;
         mu_0 /= subN;
         mu_1 /= subN;
         log2FC /= subN*log(2);
         ns_misc::computeCI(95, &difs, &ciLow, &ciHigh);
         ciLow /= log(2);
         ciHigh /= log(2);
         outFile<<pplr<<" "<<log2FC<<" "<<ciLow<<" "<<ciHigh<<" "<<mu_0<<" "<<mu_1<<endl;
      }
   }else{
      if(args.verbose)cout<<"Computing Log Ratio distribution"<<endl;
      long selectM = trSelect.size();
      outFile<<"# Log Ratio distribution"<<endl;
      outFile<<"# T "<<endl;
      outFile<<"# M "<<selectM<<endl;
      outFile<<"# N "<<subN<<endl;
      outFile<<"# first column - transcript number (zero based)"<<endl;
      for(m=0;m<selectM;m++){
         if(selectM>10)progressLog(m,M);
         cond.getTranscript(0,trSelect[m],tr[0],subN);
         cond.getTranscript(1,trSelect[m],tr[1],subN);
         outFile<<trSelect[m]<<" ";
         for(i=0;i<subN;i++){
            if(doLog){
               tr[1][i] = log(tr[1][i]);
               tr[0][i] = log(tr[0][i]);
            }
            outFile<<tr[1][i]-tr[0][i]<<" ";
         }
         outFile<<endl;
      }
   }
   outFile.close();
   cond.close();
   return 0;
}

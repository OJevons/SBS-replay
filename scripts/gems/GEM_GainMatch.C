#include "TFile.h"
#include "TTree.h"
#include "TChain.h"
//#include "GEM_cosmic_tracks.C"
#include "TString.h"
#include "TH2D.h"
#include "TH1D.h"
#include "TF1.h"
#include "TSystem.h"
#include <iostream>
#include <fstream> 
#include "TROOT.h"
#include "TClonesArray.h"
#include "TCanvas.h"
#include "TPad.h"
#include "TVectorD.h"
#include "TMatrixD.h"
//#include "Math/Functor.h"
#include "TMinuit.h"
#include "TFitResultPtr.h"
#include "TFitResult.h"
#include "TObjArray.h"
#include "TObjString.h"

vector<double> AsymALL, dAsymALL;
vector<int> APVX_asymALL, APVY_asymALL;
vector<vector<double> > weightX, weightY;

vector<int> countX,countY;

int nAPVmaxX = 12;
int nAPVmaxY = 10;

void chi2_FCN( int &npar, double *gin, double &f, double *par, int flag ){

  //cout << "calculating chi2..." << endl;
  
  double chi2 = 0.0;
  for( int i=0; i<AsymALL.size(); i++ ){
    int iy = APVY_asymALL[i];
    int ix = APVX_asymALL[i];

    bool useasym = false;
    
    double Gx=1.0, Gy=1.0;
    
    if( iy >= 0 && iy < nAPVmaxY && ix >= 0 && ix < nAPVmaxX ){
      Gx = par[ix + nAPVmaxY];
      Gy = par[iy];

      useasym = true;
    } else if( iy >= 0 && iy < nAPVmaxY ){ //this is a module-average asymmetry for Y: using weighted average over all X APVs for Gx:
      double sum_Gx = 0.0;
      double sum_Wx = 0.0;
      for( int j = 0; j<nAPVmaxX; j++ ){
	sum_Gx += weightX[iy][j] * par[j+nAPVmaxY];
	sum_Wx += weightX[iy][j];
      }

      Gx = sum_Gx/sum_Wx;
      Gy = par[iy];

      //only use the module-average asymmetry in the fit if there were no X APVs with at least 100 events on this Y APV:
      // 
      // if( countX[iy] == 0 )

      //Let's see what happens if we use the module-average asymmetry here:
      if( countX[iy] == 0 ) useasym = true;
      
    } else if( ix >= 0 && ix < nAPVmaxX ){ //this is a module-average asymmetry for X: using weighted average over all Y APVs for Gy:
      double sum_Gy = 0.0;
      double sum_Wy = 0.0;
      for( int j=0; j<nAPVmaxY; j++ ){
	sum_Gy += weightY[ix][j] * par[j];
	sum_Wy += weightY[ix][j];
      }

      Gx = par[ix+nAPVmaxY];
      Gy = sum_Gy/sum_Wy;

      if( countY[ix] == 0 ) useasym = true;
	
    }
      
    double Atheory = (Gx - Gy)/(Gx + Gy);

    if( useasym ) chi2 += pow( (AsymALL[i] - Atheory)/dAsymALL[i], 2 );
    
  }

  //cout << "chi2 = " << chi2 << endl;
  
  f = chi2;
}


void GEM_GainMatch( const char *infilename, int nmodules, const char *detname="bb.gem", double chi2cut=100.0, double ADCcut = 1500.0, double target_ADC=4000.0, const char *fname_stripconfig="stripconfig_bb_gem.txt" ){

  ifstream stripconfigfile(fname_stripconfig);

  TString currentline;

  bool gotxconfig = false;
  bool gotyconfig = false;
  
  vector<int> nstripx_mod(nmodules);
  vector<int> nstripy_mod(nmodules);

  while( currentline.ReadLine(stripconfigfile) ){
    if( currentline.BeginsWith("mod_nstripu") ){
      TObjArray *tokens = currentline.Tokenize(" ");
      if( tokens->GetEntries() >= nmodules+1 ){
	for( int i=1; i<=nmodules; i++ ){
	  nstripx_mod[i-1] = ( (TObjString*) (*tokens)[i] )->GetString().Atoi();
	}
	gotxconfig = true;
      }
    }

    if( currentline.BeginsWith( "mod_nstripv" ) ){
      TObjArray *tokens = currentline.Tokenize( " " );
      if( tokens->GetEntries() >= nmodules+1 ){
	for( int i=1; i<=nmodules; i++ ){
	  nstripy_mod[i-1] = ( (TObjString*) (*tokens)[i] )->GetString().Atoi();
	  
	}
	gotyconfig = true;
      }
    }
  }

  if( !( gotxconfig && gotyconfig ) ){
    cout << "module strip config not properly defined, quitting..." << endl;
    return;
  }

  int nstripxmax = 0, nstripymax=0; 
  for( int i=0; i<nmodules; i++ ){
    nstripxmax = nstripx_mod[i] > nstripxmax ? nstripx_mod[i] : nstripxmax;
    nstripymax = nstripy_mod[i] > nstripymax ? nstripy_mod[i] : nstripymax; 
  }
  

  gROOT->ProcessLine(".x ~/rootlogon.C");

  UInt_t MAXNHITS=10000;
  
  TString fname(infilename);

  TChain *C = new TChain("T");

  C->Add( infilename );

  C->Print();
  
  TString branchname;
  double ngoodhits;
  double ntracks;
  double besttrack;
  vector<double> tracknhits(MAXNHITS);
  vector<double> trackChi2NDF(MAXNHITS);
  vector<double> hit_trackindex(MAXNHITS);
  vector<double> hit_module(MAXNHITS);
  vector<double> hit_layer(MAXNHITS);
  vector<double> hit_nstripu(MAXNHITS);
  vector<double> hit_nstripv(MAXNHITS);
  vector<double> hit_ustripmax(MAXNHITS);
  vector<double> hit_ustriplo(MAXNHITS);
  vector<double> hit_ustriphi(MAXNHITS);
  vector<double> hit_vstripmax(MAXNHITS);
  vector<double> hit_vstriplo(MAXNHITS);
  vector<double> hit_vstriphi(MAXNHITS);
  vector<double> hit_ADCU(MAXNHITS);
  vector<double> hit_ADCV(MAXNHITS);
  vector<double> hit_ADCavg(MAXNHITS);
  vector<double> hit_ADCasym(MAXNHITS);
  vector<double> hit_ADCmaxsampU(MAXNHITS);
  vector<double> hit_ADCmaxsampV(MAXNHITS);
  vector<double> hit_ADCmaxstripU(MAXNHITS);
  vector<double> hit_ADCmaxstripV(MAXNHITS);

  map<TString,TString> branchnames;
  vector<TString> varnames;
  varnames.push_back("track.ntrack");
  varnames.push_back("track.nhits");
  varnames.push_back("track.besttrack");
  varnames.push_back("track.chi2ndf");
  varnames.push_back("hit.ngoodhits");
  varnames.push_back("hit.trackindex");
  varnames.push_back("hit.module");
  varnames.push_back("hit.layer");
  varnames.push_back("hit.nstripu");
  varnames.push_back("hit.nstripv");
  varnames.push_back("hit.ustripmax");
  varnames.push_back("hit.ustriplo");
  varnames.push_back("hit.ustriphi");
  varnames.push_back("hit.vstripmax");
  varnames.push_back("hit.vstriplo");
  varnames.push_back("hit.vstriphi");
  varnames.push_back("hit.ADCU");
  varnames.push_back("hit.ADCV");
  varnames.push_back("hit.ADCavg");
  varnames.push_back("hit.ADCasym");
  varnames.push_back("hit.ADCmaxsampU");
  varnames.push_back("hit.ADCmaxsampV");
  varnames.push_back("hit.ADCmaxstripU");
  varnames.push_back("hit.ADCmaxstripV");

  cout << "disabling all branches...";
  
  C->SetBranchStatus("*",0);

  cout << "done." << endl;

  for( int i=0; i<varnames.size(); i++ ){
    branchnames[varnames[i]] = branchname.Format("%s.%s",detname,varnames[i].Data());
    cout << "Branch " << i << " name = " << branchnames[varnames[i]] << endl;
    C->SetBranchStatus( branchnames[varnames[i]].Data(), 1 );
  }

  cout << "Setting branch addresses: ";
  C->SetBranchAddress( branchnames["track.ntrack"].Data(), &ntracks );
  C->SetBranchAddress( branchnames["track.besttrack"].Data(), &besttrack );
  C->SetBranchAddress( branchnames["track.nhits"].Data(), &(tracknhits[0]) );
  C->SetBranchAddress( branchnames["track.chi2ndf"].Data(), &(trackChi2NDF[0]) );
  C->SetBranchAddress( branchnames["hit.ngoodhits"].Data(), &ngoodhits );
  C->SetBranchAddress( branchnames["hit.trackindex"].Data(), &(hit_trackindex[0]) );
  C->SetBranchAddress( branchnames["hit.module"].Data(), &(hit_module[0]) );
  C->SetBranchAddress( branchnames["hit.layer"].Data(), &(hit_layer[0]) );
  C->SetBranchAddress( branchnames["hit.nstripu"].Data(), &(hit_nstripu[0]) );
  C->SetBranchAddress( branchnames["hit.nstripv"].Data(), &(hit_nstripv[0]) );
  C->SetBranchAddress( branchnames["hit.ustripmax"].Data(), &(hit_ustripmax[0]) );
  C->SetBranchAddress( branchnames["hit.ustriplo"].Data(), &(hit_ustriplo[0]) );
  C->SetBranchAddress( branchnames["hit.ustriphi"].Data(), &(hit_ustriphi[0]) );
  C->SetBranchAddress( branchnames["hit.vstripmax"].Data(), &(hit_vstripmax[0]) );
  C->SetBranchAddress( branchnames["hit.vstriplo"].Data(), &(hit_vstriplo[0]) );
  C->SetBranchAddress( branchnames["hit.vstriphi"].Data(), &(hit_vstriphi[0]) );
  C->SetBranchAddress( branchnames["hit.ADCU"].Data(), &(hit_ADCU[0]) );
  C->SetBranchAddress( branchnames["hit.ADCV"].Data(), &(hit_ADCV[0]) );
  C->SetBranchAddress( branchnames["hit.ADCavg"].Data(), &(hit_ADCavg[0]) );
  C->SetBranchAddress( branchnames["hit.ADCasym"].Data(), &(hit_ADCasym[0]) );
  C->SetBranchAddress( branchnames["hit.ADCmaxsampU"].Data(), &(hit_ADCmaxsampU[0]) );
  C->SetBranchAddress( branchnames["hit.ADCmaxsampV"].Data(), &(hit_ADCmaxsampV[0]) );
  C->SetBranchAddress( branchnames["hit.ADCmaxstripU"].Data(), &(hit_ADCmaxstripU[0]) );
  C->SetBranchAddress( branchnames["hit.ADCmaxstripV"].Data(), &(hit_ADCmaxstripV[0]) );
  cout << "done." << endl;
  
  TString outfilename = Form("GainRatios_%s_temp.root",detname); 

  

  //  outfilename.Prepend("GainRatios_");

  cout << "Out file name = " << outfilename << endl;

  TFile *fout = new TFile( outfilename.Data(), "RECREATE" );
  
  outfilename.ReplaceAll(".root",".txt");

  ofstream outfile(outfilename.Data());

  TString dbfilename;
  dbfilename.Form( "GEM_GainMatchResults_%s.dat",detname );
  
  ofstream outfile_db(dbfilename.Data());
  
  nAPVmaxX = nstripxmax/128;
  if( nstripxmax % 128 > 0 ) nAPVmaxX++;

  nAPVmaxY = nstripymax/128;
  if( nstripymax % 128 > 0 ) nAPVmaxY++;
  
  TClonesArray *hADCasym_vs_APVXY = new TClonesArray( "TH1D", nAPVmaxX*nAPVmaxY*nmodules );
  TClonesArray *hADCasym_vs_APVX = new TClonesArray( "TH1D", nAPVmaxX*nmodules );
  TClonesArray *hADCasym_vs_APVY = new TClonesArray( "TH1D", nAPVmaxY*nmodules );
  
  for( int imodule=0; imodule<nmodules; imodule++ ){
    TString hname;
    for( int iAPVx = 0; iAPVx < nAPVmaxX; iAPVx++ ){
      hname.Form( "hADCasym_vs_APVX%d_mod%d", iAPVx, imodule );

      if( iAPVx*128 < nstripx_mod[imodule] ){
      
	new( (*hADCasym_vs_APVX)[iAPVx+nAPVmaxX*imodule] ) TH1D( hname.Data(), "", 250, -1.01, 1.01 );

      }
      
      for( int iAPVy = 0; iAPVy < nAPVmaxY; iAPVy++ ){

	if( iAPVy*128 < nstripy_mod[imodule] ){
	  if( iAPVx==0 ){
	    hname.Form( "hADCasym_vs_APVY%d_mod%d", iAPVy, imodule );
	    new( (*hADCasym_vs_APVY)[iAPVy+nAPVmaxY*imodule] ) TH1D( hname.Data(), "", 250, -1.01, 1.01 );
	  }
	
	  hname.Form("hADCasym_vs_APV_mod%d_x%d_y%d",imodule,iAPVx,iAPVy);
	  
	  new( (*hADCasym_vs_APVXY)[iAPVy+nAPVmaxY*iAPVx+nAPVmaxX*nAPVmaxY*imodule] ) TH1D( hname.Data(), "", 250,-1.01,1.01);
	}
      }
    }
  }
  
  //GEM_cosmic_tracks *T = new GEM_cosmic_tracks(C);
  
  long nevent=0;

  TH2D *hADCasym_module = new TH2D("hADCasym_module","",nmodules,-0.5,nmodules-0.5,500,-1.01,1.01);
  TH2D *hNstripX_module = new TH2D("hNstripX_module","",nmodules,-0.5,nmodules-0.5,12,0.5,12.5);
  TH2D *hNstripY_module = new TH2D("hNstripY_module","",nmodules,-0.5,nmodules-0.5,12,0.5,12.5);

  TH2D *hADCavg_module = new TH2D("hADCavg_module","",nmodules,-0.5,nmodules-0.5,1500,0,30000);
  TH1D *hADCavg_allhits = new TH1D("hADCavg_allhits","",1500,0,30000);

  TH2D *hStripADCsumU_module = new TH2D("hStripADCsumU_module","",nmodules,-0.5,nmodules-0.5,1500,0,15000);
  TH2D *hStripADCmaxU_module = new TH2D("hStripADCmaxU_module","",nmodules,-0.5,nmodules-0.5,1500,0,3000);
  TH2D *hStripADCsumV_module = new TH2D("hStripADCsumV_module","",nmodules,-0.5,nmodules-0.5,1500,0,15000);
  TH2D *hStripADCmaxV_module = new TH2D("hStripADCmaxV_module","",nmodules,-0.5,nmodules-0.5,1500,0,3000);
  
  //int nAPVmax = 

  //cout << "starting event loop:" << endl;
  while( C->GetEntry( nevent++ ) ){

    if( ngoodhits > MAXNHITS ) continue;
    if( ntracks > MAXNHITS ) continue;
    
    if( nevent % 1000 == 0 ) cout << "event " << nevent << endl;
    //loop over hits:
    int itrack = int(besttrack);
    //cout << "itrack = " << itrack << endl;

    int NTRACKS = int(ntracks);

    //cout << "ntracks = " << NTRACKS << endl;
    
    if( NTRACKS > 0 ){

      if( trackChi2NDF[itrack] < chi2cut && tracknhits[itrack] > 3 ){
	int nhits = int(ngoodhits);
	//	cout << "nhits = " << nhits << endl;
	
	for( int ihit=0; ihit<nhits; ihit++ ){
	  int tridx = int( hit_trackindex[ihit] );

	  //cout << "ihit, tridx = " << ihit << ", " << tridx;
	  
	  if( 0.5*(hit_ADCU[ihit]+hit_ADCV[ihit]) >= ADCcut && tridx == itrack ){
	    // cout << ", ADCavg[ihit] = " << hit_ADCavg[ihit]
	    // 	 << ", ADCasym[ihit] = " << hit_ADCasym[ihit]
	    // 	 << ", (nstripu,nstripv) = (" << hit_nstripu[ihit] << ", " << hit_nstripv[ihit] << ")"
	    // 	 << ", (ustriplo,ustriphi,ustripmax)=(" << hit_ustriplo[ihit] << ", " << hit_ustriphi[ihit] << ", " << hit_ustripmax[ihit] << ")"
	    // 	 << ", (vstriplo,vstriphi,vstripmax)=(" << hit_vstriplo[ihit] << ", " << hit_vstriphi[ihit] << ", " << hit_vstripmax[ihit] << ")"
	    // 	 << ", module = " << hit_module[ihit] << endl;
	    hADCavg_allhits->Fill( 0.5*(hit_ADCU[ihit]+hit_ADCV[ihit]) );
	    hADCavg_module->Fill( hit_module[ihit], 0.5*(hit_ADCU[ihit]+hit_ADCV[ihit]) );
	    hADCasym_module->Fill( hit_module[ihit], hit_ADCasym[ihit] );
	    hNstripX_module->Fill( hit_module[ihit], hit_nstripu[ihit] );
	    hNstripY_module->Fill( hit_module[ihit], hit_nstripv[ihit] );

	    hStripADCsumU_module->Fill( hit_module[ihit], hit_ADCmaxstripU[ihit] );
	    hStripADCsumV_module->Fill( hit_module[ihit], hit_ADCmaxstripV[ihit] );

	    hStripADCmaxU_module->Fill( hit_module[ihit], hit_ADCmaxsampU[ihit] );
	    hStripADCmaxV_module->Fill( hit_module[ihit], hit_ADCmaxsampV[ihit] );

	    int ixlo = hit_ustriplo[ihit];
	    int ixhi = hit_ustriphi[ihit];
	    int ixmax = hit_ustripmax[ihit];

	    int iylo = hit_vstriplo[ihit];
	    int iyhi = hit_vstriphi[ihit];
	    int iymax = hit_vstripmax[ihit];

	    int xAPVmax = ixmax/128;
	    int yAPVmax = iymax/128;
	    int xAPVlo = ixlo/128;
	    int yAPVlo = iylo/128;

	    int xAPVhi = ixhi/128;
	    int yAPVhi = iyhi/128;

	    int module = int(hit_module[ihit]);
	  
	    if( xAPVlo == xAPVmax && xAPVhi == xAPVmax &&
		yAPVlo == yAPVmax && yAPVhi == yAPVmax &&
		hit_nstripu[ihit] >= 2 && hit_nstripv[ihit] >= 2 &&
		xAPVmax*128 < nstripx_mod[module] && yAPVmax*128<nstripy_mod[module] ){

	      // cout << "filling histograms " << endl;

	      // cout << "apvxy hist index = " << yAPVmax + nAPVmaxY*xAPVmax+nAPVmaxX*nAPVmaxY*module << endl;
	      // cout << "apvx hist index = " << xAPVmax + nAPVmaxX*module << endl;
	      // cout << "apvy hist index = " << yAPVmax + nAPVmaxY*module << endl;
	      
	      ( (TH1D*) (*hADCasym_vs_APVXY)[yAPVmax + nAPVmaxY*xAPVmax+nAPVmaxX*nAPVmaxY*module] )->Fill( hit_ADCasym[ihit] );

	      ( (TH1D*) (*hADCasym_vs_APVX)[xAPVmax + nAPVmaxX*module] )->Fill( hit_ADCasym[ihit] );
	      ( (TH1D*) (*hADCasym_vs_APVY)[yAPVmax + nAPVmaxY*module] )->Fill( hit_ADCasym[ihit] );
	      //cout << "done histogram fill" << endl;
	    
	    }
	  }
	////
	}
      }
    }
    //cout << "Event " << nevent << " done" << endl;
  }

  TFitResultPtr fitadcall = hADCavg_allhits->Fit("landau","S","",ADCcut,25000.);

  double MPV_all = ( (TF1*) hADCavg_allhits->GetListOfFunctions()->FindObject("landau") )->GetParameter("MPV");

  cout << "All hits ADC peak position = " << MPV_all << endl;

  
  

  double asympeak[nmodules];
  double R[nmodules];
  
  TH1D *htemp;

  //outfile << "mod_RYX     ";

  TCanvas *c1 = new TCanvas("c1","c1",2000,1000);
  c1->Divide(2,1);
  c1->cd(1);

  hADCasym_module->Draw("colz");

  gPad->Modified();
  c1->Update();
  
  c1->cd(2);

  vector<double> RelativeGainByModule(nmodules);
  
  for( int i=0; i<nmodules; i++ ){
    TString hnametemp;
    hnametemp.Form("ADCasym_module%d",i);
    htemp = hADCasym_module->ProjectionY(hnametemp.Data(), i+1, i+1 );

    if( htemp->GetEntries() >= 100 ){
      
      double max = htemp->GetMaximum();
      int binmax,binlo,binhi;
      
      binmax = htemp->GetMaximumBin();
      binlo = binmax;
      binhi = binmax;
      
      while( htemp->GetBinContent(binlo) >= 0.5*max && binlo > 1 ){binlo--;}
      while( htemp->GetBinContent(binhi) >= 0.5*max && binhi < 500 ){binhi++; }
      
      
      
      htemp->Fit("gaus","S","",htemp->GetBinCenter(binlo),htemp->GetBinCenter(binhi) );
      
      TF1 *fitfunc = (TF1*) htemp->GetListOfFunctions()->FindObject("gaus");
      
      asympeak[i] = fitfunc->GetParameter(1);
      R[i] = (1.0-asympeak[i])/(1.0+asympeak[i]);
      
      gPad->Modified();
      c1->Update();
      
      //      gSystem->Sleep(250);
      TString outstring;
    //outfile << outstring.Format(" %12.6g ", R[i] );

    //Within each module, we measure an asymmetry for all possible combinations of X APV (or "U") and Y APV (or "V"):

    //Let us find the gain coefficients for each individual APV card that minimize the chi^2 defined as the
    // sum of squared differences between the modified ADC asymmetries and zero:
    // If APV card X has relative gain Gx and card Y has relative gain Gy, then the observed ADC asymmetry would be:
    // ASYM = ( Gx - Gy )/(Gx + Gy ) = (1 - Ryx_i)/(1 + Ryx_i)
    // Let's choose some reference APVX as having gain = 1. So all gains are measured relative to this common
    // reference. 
    // Ryx = (1-A)/(1+A),
    // For sufficiently small A, Ryx - 1 = (1-A)/(1+A) - 1 = -2A/(1+A) ~= -2A
    //We have asymmetries A_ij 
    }

    hnametemp.Form("ADCdist_module%d", i );

    htemp = hADCavg_module->ProjectionY( hnametemp.Data(), i+1, i+1 );

    if( htemp->GetEntries() >= 100. ){

      TFitResultPtr ADCfit_module = htemp->Fit("landau","S","",ADCcut,25000.0);
      double MPV_mod = ( (TF1*) (htemp->GetListOfFunctions()->FindObject("landau") ) )->GetParameter("MPV");

      cout << "module " << i << " MPV = " << MPV_mod << endl;
      cout << "module " << i << " relative gain = " << MPV_mod / target_ADC << endl;
      RelativeGainByModule[i] = MPV_mod/MPV_all;
    } else {
      RelativeGainByModule[i] = 1.0;
    }
  }


  outfile_db << endl << "#Module internal relative gains by APV card: " << endl;

  map<int,vector<double> > Xgain_by_module, Ygain_by_module;
  
  for( int i=0; i<nmodules; i++ ){
    int xAPV_ref = nAPVmaxX/2;
      
      //First, determine all the Y gains relative to the reference APV,
    // A_{ij} = (1-R_{ij})/(1+R_{ij})
    // Ryx_{ij] = (1-A_{ij})/(1+A_{ij}) = Gyi/Gxj
    // 
    //chi^2 = sum_i,j (A_{ij}^(measured) - A_{ij}^{calc})^2/(Delta A_{ij}^{measured})^2
    // Is there a way to make this problem linear?
    // Maybe:
    //For sufficiently small values of R_{yx} - 1, we have:
    
    //A_{ij}^{calc} = (1 - Ryx_{ij})/(1+ Ryx_{ij}) 

    vector<double> Asym, dAsym;
    vector<double> Ryx, dRyx;

    vector<double> Xgain, dXgain, Ygain, dYgain; //by APV within this module:

    double minAsymerr = 1000.0;

    int count=0;

    AsymALL.clear();
    dAsymALL.clear();

    TMinuit *gainfit = new TMinuit( nAPVmaxX+nAPVmaxY );

    TString fitname;
    gainfit->SetName( fitname.Format("gainfit_module%d",i) ); 
    
    gainfit->SetFCN(chi2_FCN);
    
    double arglist[10];
    
    int ierflg=0;
    
    for( int ipar=0; ipar<nAPVmaxY; ipar++ ){
      TString parname;
      parname.Form("Gain_APVY%d",ipar);
      
      gainfit->mnparm( ipar, parname.Data(), 1.0, 0.1,0,0,ierflg ); 
      
    }
    
    for( int ipar=0; ipar<nAPVmaxX; ipar++ ){
      TString parname;
      parname.Form("Gain_APVX%d",ipar);
      
      gainfit->mnparm( ipar+nAPVmaxY, parname.Data(), 1.0, 0.1,0,0,ierflg ); 
      
    }
    AsymALL.clear();
    dAsymALL.clear();

    APVX_asymALL.clear();
    APVY_asymALL.clear();
    
    weightX.clear();
    weightY.clear();

    weightX.resize(nAPVmaxY);
    weightY.resize(nAPVmaxX);
    //Shall we just create a separate TMinuit object for each module? Why TF not?

    countX.resize(nAPVmaxY);
    countY.resize(nAPVmaxX); 

    xAPV_ref = -1;
    double maxnevent = 0.0;
    
    //Card-average asymmetries:
    for( int ix=0; ix<nAPVmaxX; ix++ ){
      weightY[ix].resize(nAPVmaxY);

      countY[ix] = 0;


      if( ix*128 < nstripx_mod[i] ){
      
	TH1D *htemp = ( (TH1D*) (*hADCasym_vs_APVX)[ix+nAPVmaxX*i] );
	if( htemp->GetEntries() >= 100 ){
	  
	  if( xAPV_ref < 0 || htemp->GetEntries() > maxnevent ){
	    xAPV_ref = ix;
	    maxnevent = htemp->GetEntries();
	  }
	  
	  double Amean = htemp->GetMean();
	  double dAmean = htemp->GetMeanError();
	  
	  double Arms = htemp->GetRMS();
	  double dArms = htemp->GetRMSError();
	  
	  htemp->Fit("gaus","SQ","",Amean-Arms, Amean+Arms);
	  
	  double Amean_fit = htemp->GetFunction("gaus")->GetParameter("Mean");
	  double dAmean_fit = htemp->GetFunction("gaus")->GetParError(1);
	  
	  double Arms_fit = htemp->GetFunction("gaus")->GetParameter("Sigma");
	  double dArms_fit = htemp->GetFunction("gaus")->GetParError(2);
	  
	  //re-run the fit with a tighter range set by the result of the first fit:
	  
	  htemp->Fit( "gaus", "SQ", "", Amean_fit - 2.0*Arms_fit, Amean_fit + 2.0*Arms_fit );
	  
	  Amean_fit = htemp->GetFunction("gaus")->GetParameter("Mean");
	  dAmean_fit = htemp->GetFunction("gaus")->GetParError(1);
	  
	  Arms_fit = htemp->GetFunction("gaus")->GetParameter("Sigma");
	  dArms_fit = htemp->GetFunction("gaus")->GetParError(2);
	  
	  double Ryxtemp = (1.0-Amean_fit)/(1.0+Amean_fit);
	  
	  double Aup = Amean_fit + dAmean_fit;
	  double Adown = Amean_fit - dAmean_fit;
	  
	  double Ryx_Aup = (1.0 - Aup)/(1.0 + Aup );
	  double Ryx_Adown = (1.0 - Adown)/(1.0 + Adown);
	  
	  AsymALL.push_back( Amean_fit );
	  dAsymALL.push_back( dAmean_fit );
	  
	  APVX_asymALL.push_back( ix );
	  APVY_asymALL.push_back( -1 );
	} else { //If less than 100 entries for this entire APV, fix the gain to 1:
	  gainfit->FixParameter( ix + nAPVmaxY );
	}
      } else {
	gainfit->FixParameter( ix + nAPVmaxY );
      }
    }

    //always fix the gain of the X APV with largest statistics to 1, to fix the absolute normalization
    gainfit->FixParameter( xAPV_ref + nAPVmaxY );
    
    for( int iy=0; iy<nAPVmaxY; iy++ ){
      weightX[iy].resize( nAPVmaxX );

      countX[iy] = 0;

      if( iy * 128 < nstripy_mod[i] ){
	
	TH1D *htemp = ( (TH1D*) (*hADCasym_vs_APVY)[iy+nAPVmaxY*i] );
	if( htemp->GetEntries() >= 100 ){
	  double Amean = htemp->GetMean();
	  double dAmean = htemp->GetMeanError();

	  double Arms = htemp->GetRMS();
	  double dArms = htemp->GetRMSError();

	  htemp->Fit("gaus","SQ","",Amean-Arms, Amean+Arms);
	 
	  double Amean_fit = htemp->GetFunction("gaus")->GetParameter("Mean");
	  double dAmean_fit = htemp->GetFunction("gaus")->GetParError(1);

	  double Arms_fit = htemp->GetFunction("gaus")->GetParameter("Sigma");
	  double dArms_fit = htemp->GetFunction("gaus")->GetParError(2);

	  //re-run the fit with a tighter range set by the result of the first fit:

	  htemp->Fit( "gaus", "SQ", "", Amean_fit - 2.0*Arms_fit, Amean_fit + 2.0*Arms_fit );

	  Amean_fit = htemp->GetFunction("gaus")->GetParameter("Mean");
	  dAmean_fit = htemp->GetFunction("gaus")->GetParError(1);
	  
	  Arms_fit = htemp->GetFunction("gaus")->GetParameter("Sigma");
	  dArms_fit = htemp->GetFunction("gaus")->GetParError(2);

	  double Ryxtemp = (1.0-Amean_fit)/(1.0+Amean_fit);

	  double Aup = Amean_fit + dAmean_fit;
	  double Adown = Amean_fit - dAmean_fit;
	  
	  double Ryx_Aup = (1.0 - Aup)/(1.0 + Aup );
	  double Ryx_Adown = (1.0 - Adown)/(1.0 + Adown);

	  AsymALL.push_back( Amean_fit );
	  dAsymALL.push_back( dAmean_fit );

	  APVX_asymALL.push_back( -1 );
	  APVY_asymALL.push_back( iy );
	} else {
	  gainfit->FixParameter( iy );
	}
      } else {
	gainfit->FixParameter( iy );
      }
    }
    
    for( int ix = 0; ix<nAPVmaxX; ix++ ){
      
      for( int iy = 0; iy<nAPVmaxY; iy++ ){

	if( ix * 128 < nstripx_mod[i] && iy*128 < nstripy_mod[i] ){
	
	  TH1D *htemp = ( (TH1D*) (*hADCasym_vs_APVXY)[iy + nAPVmaxY*ix + nAPVmaxY*nAPVmaxX*i] );

	
	
	  weightX[iy][ix] = htemp->GetEntries();
	  weightY[ix][iy] = htemp->GetEntries();
	
	  if( htemp->GetEntries() >= 100 ){

	    countY[ix]++;
	    countX[iy]++;
	  
	    double Amean = htemp->GetMean();
	    double dAmean = htemp->GetMeanError();

	    double Arms = htemp->GetRMS();
	    double dArms = htemp->GetRMSError();

	    htemp->Fit("gaus","SQ","",Amean-Arms, Amean+Arms);
	 
	    double Amean_fit = htemp->GetFunction("gaus")->GetParameter("Mean");
	    double dAmean_fit = htemp->GetFunction("gaus")->GetParError(1);

	    double Arms_fit = htemp->GetFunction("gaus")->GetParameter("Sigma");
	    double dArms_fit = htemp->GetFunction("gaus")->GetParError(2);

	    //re-run the fit with a tighter range set by the result of the first fit:

	    htemp->Fit( "gaus", "SQ", "", Amean_fit - 2.0*Arms_fit, Amean_fit + 2.0*Arms_fit );

	    Amean_fit = htemp->GetFunction("gaus")->GetParameter("Mean");
	    dAmean_fit = htemp->GetFunction("gaus")->GetParError(1);
	  
	    Arms_fit = htemp->GetFunction("gaus")->GetParameter("Sigma");
	    dArms_fit = htemp->GetFunction("gaus")->GetParError(2);

	    double Ryxtemp = (1.0-Amean_fit)/(1.0+Amean_fit);

	    double Aup = Amean_fit + dAmean_fit;
	    double Adown = Amean_fit - dAmean_fit;
	  
	    double Ryx_Aup = (1.0 - Aup)/(1.0 + Aup );
	    double Ryx_Adown = (1.0 - Adown)/(1.0 + Adown);

	  
	  
	    Asym.push_back( Amean_fit );
	    dAsym.push_back( dAmean_fit );
	    Ryx.push_back( Ryxtemp );
	    dRyx.push_back( 0.5*fabs(Ryx_Aup - Ryx_Adown) );

	    //ONLY populate the list of asymmetries IF we had a successful fit:
	    AsymALL.push_back( Amean_fit );
	    dAsymALL.push_back( dAmean_fit );

	    APVX_asymALL.push_back( ix );
	    APVY_asymALL.push_back( iy );
	  
	    if( count == 0 || dAmean_fit < minAsymerr ){
	      minAsymerr = dAmean_fit;
	      xAPV_ref = ix;
	      count++;
	    }
	  } else {
	    Asym.push_back( -10.0 );
	    dAsym.push_back( 1.0 );
	    Ryx.push_back( 1.0 );
	    dRyx.push_back( 1.0 );
	  }
	} else {
	  weightX[iy][ix] = 0.0;
	  weightY[ix][iy] = 0.0;
	}
      }
    }

    arglist[0] = 5000;
    arglist[1] = 0.1;
    gainfit->mnexcm("MIGRAD",arglist,2,ierflg);
    
    //First calculate all the Y gains relative to the reference x APV

    double Gmod = RelativeGainByModule[i];
    
    outfile << "mod_Ygain  " << i << "   " << nAPVmaxY << "     ";

    TString varname;
    varname.Form("%s.m%d.vgain = ",detname,i);

    outfile_db << varname;
    
    for( int iy=0; iy<nAPVmaxY; iy++ ){
      int yidx= iy + nAPVmaxY * xAPV_ref;
      
      // Ygain.push_back( Ryx[yidx] );
      // dYgain.push_back( dRyx[yidx] );

      double Gy, dGy;
      gainfit->GetParameter( iy, Gy, dGy );
      Ygain.push_back( Gy );
      dYgain.push_back( dGy );
      
      cout << "module " << i << ", Y APV " << iy << ", Relative gain = " << Ygain.back() << " +/- " << dYgain.back() << endl;

      outfile << Ygain.back() << "  ";

      if( iy*128 < nstripy_mod[i] ){
	outfile_db << Ygain.back() << "  ";
	Ygain_by_module[i].push_back( Ygain.back() );
      }
    }
    outfile << endl;
    outfile_db << endl;
    
    outfile << "mod_Xgain  " << i << "   " << nAPVmaxX << "     ";
    varname.Form("%s.m%d.ugain = ",detname,i);

    outfile_db << varname;
    
    for( int ix=0; ix<nAPVmaxX; ix++ ){

      double Gx, dGx;
      gainfit->GetParameter( ix + nAPVmaxY, Gx, dGx );
      Xgain.push_back( Gx );
      dXgain.push_back( dGx );
      // if( ix != xAPV_ref ){
      // 	double sum_xgain = 0.0;
      // 	double sum_weights = 0.0;
      // 	//to get relative X gains, compute a weighted average of the relative X gains from
      // 	//the relative Y gains: 
      // 	for( int iy=0; iy<nAPVmaxY; iy++ ){
      // 	  int idx = iy + nAPVmaxY*ix;
      // 	  if( fabs( Asym[idx] ) <= 1.0 ){ //then we have successfully fit the asymmetry for this combination:
      // 	    double ygaintemp = Ygain[iy];

      // 	    double dygaintemp = dYgain[iy];

      // 	    double xgaintemp = Ygain[iy]/Ryx[idx];
      // 	    double dxgaintemp = fabs(xgaintemp)*sqrt(pow( dygaintemp/ygaintemp, 2 ) + pow( dRyx[idx]/Ryx[idx],2));

      // 	    sum_xgain += xgaintemp * pow( dxgaintemp, -2 );
      // 	    sum_weights += pow( dxgaintemp, -2 );
      // 	  }
      // 	}

      // 	if( sum_xgain > 0.0 ){
      // 	  Xgain.push_back( sum_xgain/sum_weights );
      // 	  dXgain.push_back( 1.0/sqrt(sum_weights) );
      // 	} else {
      // 	  Xgain.push_back( 1.0 );
      // 	  dXgain.push_back( 1.0 );
      // 	}
      // } else {
      // 	Xgain.push_back( 1.0 );
      // 	dXgain.push_back( 0.0 );
      // }
      cout << "module " << i << ", X APV " << ix << ", Relative gain = " << Xgain.back() << " +/- " << dXgain.back() << endl;

      outfile << Xgain.back() << "  ";
      if( ix*128 < nstripx_mod[i] ){
	outfile_db << Xgain.back() << "  ";
	Xgain_by_module[i].push_back( Xgain.back() );
      }
    }
    outfile << endl;
    outfile_db << endl;

    //outfile_db << "# Module " << i << " average gain relative to target ADC of " << target_ADC << " = " << Gmod << endl;
  }

  


  //We will want a 2nd loop over the data to make plots of corrected ADC spectra:

  //int nAPVmax = 

  TH1D *hADCavg_allhits_corrected = new TH1D("hADCavg_allhits_corrected","",1500,0,30000);
  TH1D *hADCasym_allhits_corrected = new TH1D("hADCasym_allhits_corrected","",250,-1.01,1.01);
  TH2D *hADCavg_module_corrected = new TH2D("hADCavg_module_corrected","",nmodules,-0.5,nmodules-0.5,1500,0,30000);

  TH2D *hStripADCsumU_module_corrected = new TH2D("hStripADCsumU_module_corrected","",nmodules,-0.5,nmodules-0.5,1500,0,15000);
  TH2D *hStripADCmaxU_module_corrected = new TH2D("hStripADCmaxU_module_corrected","",nmodules,-0.5,nmodules-0.5,1500,0,3000);
  TH2D *hStripADCsumV_module_corrected = new TH2D("hStripADCsumV_module_corrected","",nmodules,-0.5,nmodules-0.5,1500,0,15000);
  TH2D *hStripADCmaxV_module_corrected = new TH2D("hStripADCmaxV_module_corrected","",nmodules,-0.5,nmodules-0.5,1500,0,3000);

  TH2D *hADC_UV_allhits_corrected = new TH2D("hADC_UV_allhits_corrected","Cluster sum ;ADCU;ADCV",250,0,25000,250,0,25000);
  TH2D *hADC_UVmaxstrip_allhits_corrected = new TH2D("hADC_UVmaxstrip_allhits_corrected","Max Strip sum;ADCU;ADCV",250,0,15000,250,0,15000);
  TH2D *hADC_UVmaxsamp_allhits_corrected = new TH2D("hADC_UVmaxsamp_allhits_corrected","Max Strip max sample;ADCU;ADCV",250,0,3000,250,0,3000);

  nevent = 0;
  
  //cout << "starting event loop:" << endl;
  while( C->GetEntry( nevent++ ) ){

    if( ngoodhits > MAXNHITS ) continue;
    if( ntracks > MAXNHITS ) continue;
    
    if( nevent % 1000 == 0 ) cout << "event " << nevent << endl;
    //loop over hits:
    int itrack = int(besttrack);
    //cout << "itrack = " << itrack << endl;

    int NTRACKS = int(ntracks);

    //cout << "ntracks = " << NTRACKS << endl;
    
    if( NTRACKS > 0 ){

      if( trackChi2NDF[itrack] < chi2cut && tracknhits[itrack] > 3 ){
	int nhits = int(ngoodhits);
	//	cout << "nhits = " << nhits << endl;
	
	for( int ihit=0; ihit<nhits; ihit++ ){
	  int tridx = int( hit_trackindex[ihit] );

	  //cout << "ihit, tridx = " << ihit << ", " << tridx;
	  
	  //if( 0.5*(hit_ADCU[ihit]+hit_ADCV[ihit]) >= ADCcut && tridx == itrack ){
	  if( tridx == itrack ){
	    // cout << ", ADCavg[ihit] = " << hit_ADCavg[ihit]
	    // 	 << ", ADCasym[ihit] = " << hit_ADCasym[ihit]
	    // 	 << ", (nstripu,nstripv) = (" << hit_nstripu[ihit] << ", " << hit_nstripv[ihit] << ")"
	    // 	 << ", (ustriplo,ustriphi,ustripmax)=(" << hit_ustriplo[ihit] << ", " << hit_ustriphi[ihit] << ", " << hit_ustripmax[ihit] << ")"
	    // 	 << ", (vstriplo,vstriphi,vstripmax)=(" << hit_vstriplo[ihit] << ", " << hit_vstriphi[ihit] << ", " << hit_vstripmax[ihit] << ")"
	    // 	 << ", module = " << hit_module[ihit] << endl;
	    // hADCavg_allhits->Fill( 0.5*(hit_ADCU[ihit]+hit_ADCV[ihit]) );
	    // hADCavg_module->Fill( hit_module[ihit], 0.5*(hit_ADCU[ihit]+hit_ADCV[ihit]) );
	    // hADCasym_module->Fill( hit_module[ihit], hit_ADCasym[ihit] );
	    // hNstripX_module->Fill( hit_module[ihit], hit_nstripu[ihit] );
	    // hNstripY_module->Fill( hit_module[ihit], hit_nstripv[ihit] );

	    // hStripADCsumU_module->Fill( hit_module[ihit], hit_ADCmaxstripU[ihit] );
	    // hStripADCsumV_module->Fill( hit_module[ihit], hit_ADCmaxstripV[ihit] );

	    // hStripADCmaxU_module->Fill( hit_module[ihit], hit_ADCmaxsampU[ihit] );
	    // hStripADCmaxV_module->Fill( hit_module[ihit], hit_ADCmaxsampV[ihit] );

	    int ixlo = hit_ustriplo[ihit];
	    int ixhi = hit_ustriphi[ihit];
	    int ixmax = hit_ustripmax[ihit];

	    int iylo = hit_vstriplo[ihit];
	    int iyhi = hit_vstriphi[ihit];
	    int iymax = hit_vstripmax[ihit];

	    int xAPVmax = ixmax/128;
	    int yAPVmax = iymax/128;
	    int xAPVlo = ixlo/128;
	    int yAPVlo = iylo/128;

	    int xAPVhi = ixhi/128;
	    int yAPVhi = iyhi/128;

	    int module = int(hit_module[ihit]);

	    if( xAPVlo == xAPVmax && xAPVhi == xAPVmax &&
	    	yAPVlo == yAPVmax && yAPVhi == yAPVmax &&
	    	hit_nstripu[ihit] >= 2 && hit_nstripv[ihit] >= 2 &&
	    	xAPVmax*128 < nstripx_mod[module] && yAPVmax*128<nstripy_mod[module] ){

	      double Xgaintemp = Xgain_by_module[module][xAPVmax];
	      double Ygaintemp = Ygain_by_module[module][yAPVmax];
	      
	      hADCavg_allhits_corrected->Fill( 0.5*(hit_ADCU[ihit]/Xgaintemp+hit_ADCV[ihit]/Ygaintemp) );
	      hADCavg_module_corrected->Fill( hit_module[ihit], 0.5*(hit_ADCU[ihit]/Xgaintemp+hit_ADCV[ihit]/Ygaintemp) );

	      hADCasym_allhits_corrected->Fill( (hit_ADCU[ihit]/Xgaintemp-hit_ADCV[ihit]/Ygaintemp) /
						(hit_ADCU[ihit]/Xgaintemp+hit_ADCV[ihit]/Ygaintemp) );
	      
	      // hADCasym_module->Fill( hit_module[ihit], hit_ADCasym[ihit] );
	      // hNstripX_module->Fill( hit_module[ihit], hit_nstripu[ihit] );
	      // hNstripY_module->Fill( hit_module[ihit], hit_nstripv[ihit] );

	      hStripADCsumU_module_corrected->Fill( hit_module[ihit], hit_ADCmaxstripU[ihit]/Xgaintemp );
	      hStripADCsumV_module_corrected->Fill( hit_module[ihit], hit_ADCmaxstripV[ihit]/Ygaintemp );

	      hStripADCmaxU_module_corrected->Fill( hit_module[ihit], hit_ADCmaxsampU[ihit]/Xgaintemp );
	      hStripADCmaxV_module_corrected->Fill( hit_module[ihit], hit_ADCmaxsampV[ihit]/Ygaintemp );

	      hADC_UV_allhits_corrected->Fill( hit_ADCU[ihit]/Xgaintemp, hit_ADCV[ihit]/Ygaintemp );
	      hADC_UVmaxstrip_allhits_corrected->Fill( hit_ADCmaxstripU[ihit]/Xgaintemp, hit_ADCmaxstripV[ihit]/Ygaintemp );
	      hADC_UVmaxsamp_allhits_corrected->Fill( hit_ADCmaxsampU[ihit]/Xgaintemp, hit_ADCmaxsampV[ihit]/Ygaintemp );
	      
	    // if( xAPVlo == xAPVmax && xAPVhi == xAPVmax &&
	    // 	yAPVlo == yAPVmax && yAPVhi == yAPVmax &&
	    // 	hit_nstripu[ihit] >= 2 && hit_nstripv[ihit] >= 2 &&
	    // 	xAPVmax*128 < nstripx_mod[module] && yAPVmax*128<nstripy_mod[module] ){

	    //   // cout << "filling histograms " << endl;

	    //   // cout << "apvxy hist index = " << yAPVmax + nAPVmaxY*xAPVmax+nAPVmaxX*nAPVmaxY*module << endl;
	    //   // cout << "apvx hist index = " << xAPVmax + nAPVmaxX*module << endl;
	    //   // cout << "apvy hist index = " << yAPVmax + nAPVmaxY*module << endl;
	      
	    //   ( (TH1D*) (*hADCasym_vs_APVXY)[yAPVmax + nAPVmaxY*xAPVmax+nAPVmaxX*nAPVmaxY*module] )->Fill( hit_ADCasym[ihit] );

	    //   ( (TH1D*) (*hADCasym_vs_APVX)[xAPVmax + nAPVmaxX*module] )->Fill( hit_ADCasym[ihit] );
	    //   ( (TH1D*) (*hADCasym_vs_APVY)[yAPVmax + nAPVmaxY*module] )->Fill( hit_ADCasym[ihit] );
	    //   //cout << "done histogram fill" << endl;
	    }
	    // }
	  }
	////
	}
      }
    }
    //cout << "Event " << nevent << " done" << endl;
  }

  //Adjust module ABSOLUTE gain AFTER correction for RELATIVE gain:

  outfile_db << endl << "# Module average gains relative to target ADC peak position of " << target_ADC << endl;
  
  for( int i=0; i<nmodules; i++ ){
    //double Gmod = RelativeGainByModule[i]*MPV_all/target_ADC;

    double Gmod = 1.0;

    TString hname;
    hname.Form("hADCavg_corrected_module%d",i);
    htemp = hADCavg_module_corrected->ProjectionY( hname.Data(), i+1, i+1 );

    if( htemp->GetEntries() >= 10000 ){
    
      TFitResultPtr ADCfit_module = htemp->Fit("landau","S","",2000.0,25000.0);
      
      double MPV_mod = ( (TF1*) (htemp->GetListOfFunctions()->FindObject("landau") ) )->GetParameter("MPV");
      
      Gmod = MPV_mod / target_ADC;
      
      cout << "# Module " << i << " average gain relative to target ADC of " << target_ADC << " = " << Gmod << endl;
      TString dbentry;
      dbentry.Form("%s.m%d.modulegain = %g",detname,i,Gmod);
      outfile_db << dbentry << endl;
      cout << dbentry << endl;
    } else {
      TString dbentry;
      dbentry.Form("%s.m%d.modulegain = %g",detname,i,1.0);
      outfile_db << dbentry << endl;
      cout << dbentry << endl;
    }
  }

  outfile << endl;
  outfile_db << endl;

  
  fout->Write();

}

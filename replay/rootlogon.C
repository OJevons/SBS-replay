R__LOAD_LIBRARY(/home/oliver/SBS/SBS-offline/lib64/libsbs.so)
// R__LOAD_LIBRARY($SBS/libsbs.so)

void rootlogon(){

  //  gSystem->AddDynamicPath("/home/oliver/SBS/SBS-offline");
  gSystem->AddIncludePath(" -I/home/oliver/SBS/SBS-offline");

}

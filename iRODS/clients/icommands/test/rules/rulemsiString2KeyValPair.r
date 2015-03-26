myTestRule {
#Input parameter is:
#  String with %-separated key=value pair strings
#Output parameter is:
#  Key-value structure
  writeLine("stdout","Add metadata string *Str to *Path");
  msiString2KeyValPair(*Str,*Keyval);
  msiAssociateKeyValuePairsToObj(*Keyval,*Path,"-d");
  msiGetDataObjPSmeta(*Path,*Buf);
  writeBytesBuf("stdout",*Buf);
}
INPUT *Str="Tester=rods%Event=document", *Path="/tempZone/home/rods/sub1/foo1"
OUTPUT ruleExecOut

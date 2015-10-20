AddPermutationConst("HWSPANNING", true);

AddPermutationConst("SMOOTHANIM", false);
AddPermutationConst("ALPHATESTINSHADER", false);
AddPermutationConst("SOFTPARTICLES", false);
AddPermutationConst("LOCALSPACE", false);

AddPermutationConst("LIGHTING_DYNAMIC", false);
AddPermutationConst("LIGHTING_STATIC", false);
AddPermutationConst("LIGHTING_SHADOWRECEIVE", false);
AddPermutationConst("LIGHTING_DOMAINFREQ", false);
AddPermutationConst("LIGHTING_MOBILE", false);
AddPermutationConst("NORMAL_FROM_DIFF_ALPHA", false);

function IsPermutationSupported (PermutationValues, Platform)
  return true;
end

AddPermutationConstAndIncTag("HWSPANNING", true);
AddPermutationConst("SOFTPARTICLES", false);

AddPermutationConst("LIGHTING_DYNAMIC", false);
AddPermutationConst("LIGHTING_STATIC", false);
AddPermutationConst("LIGHTING_SHADOWRECEIVE", false);
AddPermutationConst("LIGHTING_DOMAINFREQ", false);
AddPermutationConst("LIGHTING_MOBILE", false);
AddPermutationBool("NORMAL_FROM_DIFF_ALPHA");

AddPermutationBool("ALPHATESTINSHADER");
AddPermutationBool("LOCALSPACE");
AddPermutationBool("SMOOTHANIM");

function IsPermutationSupported (PermutationValues, Platform)
  return true;
end



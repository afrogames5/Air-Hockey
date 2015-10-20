AddPermutationConstAndIncTag("HWSPANNING", true);

AddPermutationBool("SMOOTHANIM");
AddPermutationBool("ALPHATESTINSHADER");
AddPermutationBool("SOFTPARTICLES");
AddPermutationBool("LOCALSPACE");

AddPermutationConstAndIncTag("LIGHTING_DYNAMIC", true);
AddPermutationBool("LIGHTING_STATIC");
AddPermutationBool("LIGHTING_SHADOWRECEIVE");
AddPermutationConstAndIncTag("LIGHTING_DOMAINFREQ", true);
AddPermutationConst("LIGHTING_MOBILE", false);
AddPermutationBool("NORMAL_FROM_DIFF_ALPHA");

function IsPermutationSupported (PermutationValues, Platform)
  return true;
end

AddPermutationConstAndIncTag("HWSPANNING", true);

AddPermutationBool("SMOOTHANIM");
AddPermutationBool("ALPHATESTINSHADER");
AddPermutationBool("SOFTPARTICLES");
AddPermutationBool("LOCALSPACE");

AddPermutationConst("LIGHTING_DYNAMIC", false);
AddPermutationConst("LIGHTING_STATIC", false);
AddPermutationConst("LIGHTING_SHADOWRECEIVE", false);
AddPermutationConst("LIGHTING_DOMAINFREQ", false);
AddPermutationConst("LIGHTING_MOBILE", false);
AddPermutationConst("NORMAL_FROM_DIFF_ALPHA", false);

function IsPermutationSupported (PermutationValues, Platform)
    -- alpha test instruction in shader is only needed on some platforms
  if (PermutationValues["ALPHATESTINSHADER"] and
      (Platform == DX9 or Platform == XBOX360 or Platform == PS3 or Platform == PSP2)) 
  then
    return false;
  end
  
  return true;
end

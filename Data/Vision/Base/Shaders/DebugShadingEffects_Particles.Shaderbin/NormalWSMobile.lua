AddPermutationBool("HWSPANNING");

AddPermutationBool("ALPHATESTINSHADER");
AddPermutationBool("LOCALSPACE");

AddPermutationConst("SMOOTHANIM", false);
AddPermutationConst("SOFTPARTICLES", false);

AddPermutationConstAndIncTag("LIGHTING_DYNAMIC", true);
AddPermutationConstAndIncTag("LIGHTING_STATIC", true);
AddPermutationConst("LIGHTING_SHADOWRECEIVE", false);
AddPermutationConst("LIGHTING_DOMAINFREQ", false);
AddPermutationConstAndIncTag("LIGHTING_MOBILE", true);
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


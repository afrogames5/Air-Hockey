AddPermutationConstAndIncTag("HWSPANNING", true);

AddPermutationBool("ALPHATESTINSHADER");
AddPermutationBool("LOCALSPACE");

AddPermutationConst("SMOOTHANIM", false);
AddPermutationConst("SOFTPARTICLES", false);

AddPermutationBool("LIGHTING_DYNAMIC");
AddPermutationBool("LIGHTING_STATIC");
AddPermutationBool("LIGHTING_SHADOWRECEIVE");
AddPermutationConst("LIGHTING_DOMAINFREQ", false);
AddPermutationConst("LIGHTING_MOBILE", false);
AddPermutationBool("NORMAL_FROM_DIFF_ALPHA");

function IsPermutationSupported (PermutationValues, Platform)
  if not PermutationValues["LIGHTING_DYNAMIC"] and not PermutationValues["LIGHTING_STATIC"]
  then
    return false;
  end

  if PermutationValues["LIGHTING_SHADOWRECEIVE"] and not PermutationValues["LIGHTING_DYNAMIC"]
  then
    return false;
  end

  -- alpha test instruction in shader is only needed on some platforms
  if (PermutationValues["ALPHATESTINSHADER"] and
      (Platform == DX9 or Platform == XBOX360 or Platform == PS3 or Platform == PSP2)) 
  then
    return false;
  end
  
  return true;
end
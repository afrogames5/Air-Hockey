AddPermutationBool("HWSPANNING");

AddPermutationBool("ALPHATESTINSHADER");
AddPermutationBool("LOCALSPACE");

AddPermutationConst("SMOOTHANIM", false);
AddPermutationConst("SOFTPARTICLES", false);

AddPermutationBool("LIGHTING_DYNAMIC");
AddPermutationBool("LIGHTING_STATIC");
AddPermutationConst("LIGHTING_SHADOWRECEIVE", false);
AddPermutationConst("LIGHTING_DOMAINFREQ", false);
AddPermutationConstAndIncTag("LIGHTING_MOBILE", true);
AddPermutationConst("NORMAL_FROM_DIFF_ALPHA", false);

function IsPermutationSupported (PermutationValues, Platform)
  if not PermutationValues["LIGHTING_DYNAMIC"] and not PermutationValues["LIGHTING_STATIC"]
  then
    return false;
  end

  -- SoftParticles and Smoothanim need hardwarespanning (because of our implementation)
  if ((PermutationValues["SOFTPARTICLES"] or PermutationValues["SMOOTHANIM"]) and 
    not PermutationValues["HWSPANNING"])
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


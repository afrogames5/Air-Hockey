AddPermutationBool("HWSPANNING");

AddPermutationBool("SMOOTHANIM");
AddPermutationBool("ALPHATESTINSHADER");
AddPermutationBool("LOCALSPACE");

AddPermutationConst("SOFTPARTICLES", false);

AddPermutationConst("LIGHTING_DYNAMIC", false);
AddPermutationConst("LIGHTING_STATIC", false);
AddPermutationConst("LIGHTING_SHADOWRECEIVE", false);
AddPermutationConst("LIGHTING_DOMAINFREQ", false);
AddPermutationConst("LIGHTING_MOBILE", false);
AddPermutationConst("NORMAL_FROM_DIFF_ALPHA", false);

function IsPermutationSupported (PermutationValues, Platform)

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
  
  -- no support for soft particles on mobile
  if (PermutationValues["SOFTPARTICLES"] and (Platform == GLES2)) 
  then
    return false;
  end
  
  -- HWSPANNING not supported on mobile
  if (PermutationValues["HWSPANNING"] and Platform == GLES2)
  then
	return false;
  end
  
  return true;
end


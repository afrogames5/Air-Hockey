AddPermutationBool("HWSPANNING");

-- Following permutations are also not available for Mobile
AddPermutationConst("SMOOTHANIM", false);
AddPermutationConst("SOFTPARTICLES", false);
AddPermutationBool("LOCALSPACE");

AddPermutationBool("ALPHATESTINSHADER");

AddPermutationBool("LIGHTING_STATIC");
AddPermutationBool("LIGHTING_DYNAMIC");
AddPermutationConst("LIGHTING_SHADOWRECEIVE", false);
AddPermutationConst("LIGHTING_DOMAINFREQ", false);
AddPermutationConstAndIncTag("LIGHTING_MOBILE", true);
AddPermutationConst("NORMAL_FROM_DIFF_ALPHA", false);

function IsPermutationSupported (PermutationValues, Platform)
  if (not PermutationValues["LIGHTING_DYNAMIC"] and not PermutationValues["LIGHTING_STATIC"])
  then
    return false;
  end

  -- alpha test instruction in shader is only needed on some platforms
  if (PermutationValues["ALPHATESTINSHADER"] and
      (Platform == DX9 or Platform == XBOX360 or Platform == PS3 or Platform == PSP2)) 
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

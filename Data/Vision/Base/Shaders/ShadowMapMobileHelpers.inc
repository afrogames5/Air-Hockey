//-----------------------------------------------------------------------------------
// ShadowMapMobileHelpers.inc
//-----------------------------------------------------------------------------------

#if defined(_VISION_GLES2)
  // Please note: do not change the two functions below unless adapting the replacement rules in ShaderSourcePatcher.json.
  float vShadow2D(in float3 shadowCoords)
  {
    return tex2D(V_SHADOW_TEX, shadowCoords.xy).r > shadowCoords.z;
  }
  
  float vShadow2DProj(in float4 shadowCoords)
  {
    return tex2Dproj(V_SHADOW_TEX, shadowCoords).r > (shadowCoords.z/ shadowCoords.w);
  }
#else
  // For DX11, hardware shadow filtering is not used since otherwise for TARGET_DOWNLEVEL_SM40_LEVEL_93 further permutations 
  // would have been necessary.
  float ComputeShadowTerm_PCF4(in float3 shadowCoords)
  {
    float2 integerPart;
    float2 fractionalPart = modf(shadowCoords.xy*ShadowMapSize.xy, integerPart);
    integerPart *= ShadowMapSize.zw;

    float4 fTexDepth;
    float2 lookUp = integerPart;
    fTexDepth.x = vTex2D(V_SHADOW_TEX, V_SHADOW_TEX_SAMPLER, lookUp).r;  
    lookUp.x = integerPart.x + ShadowMapSize.z;
    fTexDepth.y = vTex2D(V_SHADOW_TEX, V_SHADOW_TEX_SAMPLER, lookUp).r;  
    lookUp.y = integerPart.y + ShadowMapSize.w;
    fTexDepth.w = vTex2D(V_SHADOW_TEX, V_SHADOW_TEX_SAMPLER, lookUp).r; 
    lookUp.x = integerPart.x;
    fTexDepth.z = vTex2D(V_SHADOW_TEX, V_SHADOW_TEX_SAMPLER, lookUp).r; 
   
    float4 fComparisons = saturate(ceil(fTexDepth - shadowCoords.z));
    return saturate(lerp(lerp(fComparisons.x, fComparisons.y, fractionalPart.x), 
                         lerp(fComparisons.z, fComparisons.w, fractionalPart.x),
                         fractionalPart.y));                     
  }
#endif

// Simple shadow mapping
half ComputeShadowTerm(in float4 shadowCoords)
{
#if defined(_VISION_GLES2)
  $if !DIRECTIONALLIGHT
    float shadowTerm = vShadow2DProj(shadowCoords);    
  $else
    float shadowTerm = vShadow2D(shadowCoords.xyz);
  $endif
#else
  $if !DIRECTIONALLIGHT
    float shadowTerm = ComputeShadowTerm_PCF4(shadowCoords.xyz/ shadowCoords.w);    
  $else
    float shadowTerm = ComputeShadowTerm_PCF4(shadowCoords.xyz);
  $endif
#endif

$if MOBILE_SUBTRACTIVE_SHADOWMAP
  // Clip shadows on faces that are back-facing to shadow casting light. Furthermore prevent artifacts in case 
  // of subtractive shadow mapping when receiver geometry is outside of clip space.
  float outsideClipspace = abs(shadowCoords.z) > shadowCoords.w;
  if (outsideClipspace)
    return 1.0f;
  return shadowTerm;
$else
  return shadowTerm;
$endif
}

half FadeoutShadow(in half shadowTerm, in float distance, in float2 fadeoutParams)
{
  float fFadeOut = 1.0f - saturate((fadeoutParams.x - distance) * fadeoutParams.y);  
  return saturate(shadowTerm + half(fFadeOut)); 
}
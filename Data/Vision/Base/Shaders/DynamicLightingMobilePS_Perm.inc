//-----------------------------------------------------------------------------------
// DynamicLightingMobilePS_Perm.inc
//-----------------------------------------------------------------------------------

#include <Shaders/VisionCommon.inc>
#include <Shaders/ShaderHelpers.inc>

//-----------------------------------------------------------------------------------
// Textures

#ifdef _VISION_DX11

  Texture2D   BaseTexture         : register(t0);
  sampler     BaseTextureSampler  : register(s0);

  $if USE_CUBEMAP
    TextureCube CubeMap             : register(t1);
    sampler     CubeMapSampler      : register(s1);
  $endif
  
  $if MOBILE_DYNAMICLIGHTNORMALMAPPING
    Texture2D   NormalMap         : register(t2);
    sampler     NormalMapSampler  : register(s2);
  $endif
  
  $ifdef USE_SHADOWMAP
    Texture2D V_SHADOW_TEX         : register(t3);
    sampler   V_SHADOW_TEX_SAMPLER : register(s3);
  $endif
  
#else

  sampler2D   BaseTexture         : register(s0);
  
  $if USE_CUBEMAP
    samplerCUBE CubeMap           : register(s1);
  $endif
  
  $if MOBILE_DYNAMICLIGHTNORMALMAPPING
    sampler2D  NormalMap          : register(s2);
  $endif
  
  $ifdef USE_SHADOWMAP
    sampler2D  V_SHADOW_TEX     : register(s3);
  $endif

#endif

//-----------------------------------------------------------------------------------

$if MOBILE_DYNAMICLIGHTNORMALMAPPING
  // needs to be set for GetTangentSpaceNormal() in vMaterialHelpers.inc
  #define VMATERIAL_NORMALMAP
  #define NO_BC5_SUPPORT 
$endif

#include "DynamicLightingMobileHelpers.inc"
#include "vMaterialHelpers.inc"

$if USE_SHADOWMAP
  #include "ShadowMapMobileHelpers.inc" 
$endif

//-----------------------------------------------------------------------------------

half4 ps_main(VS_OUT In) : SV_Target
{
$if MOBILE_VERTEXLIGHTING

  // get interpolated color
  half4 DynamicLightColor = In.Color;
  
$elif MOBILE_DYNAMICLIGHTNORMALMAPPING

  // sample normal map
  half4 NormalMapValue = GetTangentSpaceNormal(In.UV0_FogCoord.xy);

  // light vector (light dir) is in tangent space -> no need to transform the sampled normal  
  $if DIRECTIONALLIGHT
    half4 DynamicLightColor = ComputeMobileLightingDirectional(normalize(In.vLightDirTS), 
      NormalMapValue.xyz);
  $elif POINTLIGHT
    half4 DynamicLightColor = ComputeMobileLightingPoint(In.vLightVec, NormalMapValue.xyz);
  $elif SPOTLIGHT
    half4 DynamicLightColor = ComputeMobileLightingSpot(In.vLightVec, normalize(In.vLightDirTS), 
      NormalMapValue.xyz);
  $endif
  
$else

  $if DIRECTIONALLIGHT
    half4 DynamicLightColor = ComputeMobileLightingDirectional(-LightDir, normalize(In.NormalWS));
  $elif POINTLIGHT
    half4 DynamicLightColor = ComputeMobileLightingPoint(In.vLightVec, normalize(In.NormalWS));
  $elif SPOTLIGHT
    half4 DynamicLightColor = ComputeMobileLightingSpot(In.vLightVec, -LightDir, normalize(In.NormalWS));
  $endif
    
$endif

$if USE_SHADOWMAP 
  half shadowTerm = ComputeShadowTerm(In.ShadowCoords);
  $if DIRECTIONALLIGHT
    shadowTerm = FadeoutShadow(shadowTerm, In.EyeDirWS.w, ShadowParameters.xy);
  $endif
  DynamicLightColor.rgb *= shadowTerm;
$endif

  // get basemap color
  half4 ResColor = GetDiffuseTexture(In.UV0_FogCoord.xy);
  
  $if USE_CUBEMAP
  
    // No need to normalize reflection vector since only needed for cubemap lookup.
    $if MOBILE_DYNAMICLIGHTNORMALMAPPING
	  half3 BiNormalWS = cross(In.TangentWS.xyz, In.NormalWS)*In.TangentWS.w;
      half3 BumpNormalWS = NormalMapValue.x * In.TangentWS - NormalMapValue.y * BiNormalWS + NormalMapValue.z * In.NormalWS;	  
	  half3 ReflectionVector = reflect(half3(In.EyeDirWS.xyz), BumpNormalWS);
	$else
	  half3 ReflectionVector = reflect(half3(In.EyeDirWS.xyz), In.NormalWS);	
	$endif	
    
    ResColor += half4(vTexCUBE(CubeMap, CubeMapSampler, ReflectionVector).xyz * ResColor.www, 0.0f);
	
  $endif 
  
  ResColor *= half4(DynamicLightColor.rgb, 1.0f);

  $if !NOFOG
    // use black as fog color (dynamic lighting is additive)
    ResColor.rgb *= half(1.0f) - saturate(In.UV0_FogCoord.z);
  $endif
  
  return ResColor;
}

//-----------------------------------------------------------------------------------

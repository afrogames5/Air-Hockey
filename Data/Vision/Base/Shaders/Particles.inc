///////////////////////////////////////////////////////////
// Particles.inc
//
// Permutation variables: HWSPANNING, SMOOTHANIM,
//                        ALPHATESTINSHADER, SOFTPARTICLES,
//                        LOCALSPACE,
//                        LIGHTING_DYNAMIC, LIGHTING_STATIC,
//                        LIGHTING_SHADOWRECEIVE,
//                        LIGHTING_DOMAINFREQ,
//                        LIGHTING_MOBILE,
//                        NORMAL_FROM_DIFF_ALPHA
///////////////////////////////////////////////////////////

#include <Shaders/VisionCommon.inc>
#include <Shaders/ShaderHelpers.inc>
#include <Shaders/DeferredShadingHelpers.inc>


// Note: Following Permutation currently only work with HWSPANNING/PARTICLES_USE_HARDWARESPANNING
// (mostly just dependent on our implementation to save further special cases)
// - SMOOTHANIM
// - SOFTPARTICLES
// - LOCALSPACE (performed in software)
// - any lighting option if LIGHTING_MOBILE is not active

// LIGHTING_MOBILE brings some of the more essential lighting features back even if HWSPANNING is disabled.
// Supported are:
// - LIGHTING_STATIC
// - LIGHTING_DYNAMIC

// Hidden #define permutations:
// - PARTICLES_TRAIL
//    Activates trail particles.
// - PARTICLES_GEOMETRY
//    For rendering geometry particles.
// - PIXEL_OUTPUT_OVERWRITE
//    Calls a user implemented function for final output color: OutputOverwrite(float4 color, float4 texColor, float3 lightingColor, float3 normalTangentSpace, float2 uv)
//    If PARTICLES_USE_PROJECTION is enabled, normalTangentSpace.xy will contain UVProj.xy
// - MOBILE_NORMAL_TO_COLOR
//    Only for LIGHTING_MOBILE: Will store normal in color register. Cancels out WORLDPOS_TO_COLOR.
// - WORLDPOS_TO_COLOR
//    Will store world position in color register.
// - PARTICLES_DEFERRED_OUTPUT
//    Outputs informations to GBuffer.
// - PARTICLES_USE_EYE_DIR
//    Vertex shader outputs direction to camera (used for certain simulation shaders).
// - DESATURATION
//    Desaturates particle color based on informations from a structured buffer (vertex shader, used for certain simulation shaders).


// Force use instancing when geometry particles are used and hardware spanning is supported
$ifdef HWSPANNING
  #define PARTICLES_USE_HARDWARESPANNING
$else
  #ifdef PARTICLES_GEOMETRY
    #define PARTICLES_USE_HARDWARESPANNING
  #endif 
$endif

$if defined(LIGHTING_DOMAINFREQ)
  #define PARTICLES_LIGHTING_DOMAINFREQ
$endif

// Determine normal mapping mode if normals are needed
$if !defined(LIGHTING_MOBILE) && (defined(LIGHTING_STATIC) || defined(LIGHTING_DYNAMIC))
  #define NEED_NORMALS
$else
  #ifdef PARTICLES_DEFERRED_OUTPUT
    #define NEED_NORMALS
  #endif
$endif

#if defined(NEED_NORMALS)
  $if defined(NORMAL_FROM_DIFF_ALPHA) && defined(HWSPANNING)
    #define PARTICLES_NORMAL_FROM_DIFF_ALPHA    
  $else
    #define PARTICLES_USE_NORMALMAP
  $endif
#endif

// Definitions for dynamic lighting constants
$if defined(LIGHTING_DYNAMIC) || defined(LIGHTING_STATIC)
  // Definitions for light constant blocks
  struct DynamicLightDirectional
  {
    float4 Direction;    // xyz: direction (world space), w: empty
    float4 Color;        // xyz: color, w: empty
  };
  struct DynamicLightPoint
  {
    float4 Position;    // xyz: position (world space), w: 1/radius
    float4 Color;       // xyz: color, w: empty
  };
  struct DynamicLightSpot
  {
    float4 Position;    // xyz: position (world space), w: 1/range
    float4 Direction;   // xyz: direction (world space), w: cos(angle/2)
    float4 Color;       // xyz: color, w: 1 / (1 - cos(angle/2))
  };

  // The following max light count values are hardcoded. You also need to change:
  // - VisParticleGroup_cl::s_MaxNumSupportedDynamicLightsX
  // - Also check if everything is alright with: VisParticleEffect_cl::UpdateDynamicLightConstants
  // - Realign packoffsets in DX11 Constant buffer and registers in DX9 constants
  // - For MAX_NUM_DYNAMIC_LIGHTS_DIRECTIONAL there is currently no loop since it works better with 
  //   the current shadow receive policy (one directional light shadow receive)
  #define MAX_NUM_DYNAMIC_LIGHTS_DIRECTIONAL 1
  #define MAX_NUM_DYNAMIC_LIGHTS_POINT 32
  #define MAX_NUM_DYNAMIC_LIGHTS_SPOT 32
$endif



#ifdef _VISION_DX11
  cbuffer g_GlobalConstantBufferFrame : register (b0)
  {
    float4x4 matPJ         : packoffset(c0);   // projection matrix
    float4x4 matEye        : packoffset(c4);   // eye matrix
    float4x4 matInvEye     : packoffset(c8);   // inverse eye matrix
    float4 contextCP       : packoffset(c12);  // context clip planes: near, far, 1/(far-near)
    float3 cameraPosition  : packoffset(c17);  // worldspace camera position
    float4 gAmbientCol     : packoffset(c21);  // global ambient color
  }
  cbuffer g_GlobalConstantBufferObject : register (b1)
  {
    float4x4 matMV         : packoffset(c0);   // model view matrix
    float4x4 matMVP        : packoffset(c4);   // model view projection matrix
    float4x4 matM2WS       : packoffset(c8);   // model to world space matrix
  }

#if defined(_IS_VERTEX_SHADER) || defined(_IS_HULL_SHADER) || defined(_IS_DOMAIN_SHADER)
    cbuffer g_GlobalConstantBufferParticles : register (b4)
    {
      float4   fadeParams    : packoffset(c0);  // particle fade parameter: near, far, 1/(far-near)
      float4   vSubDivAspect : packoffset(c1);  // particle animation parameter: xy:animsubdiv, z:aspect (but not used), w:repetitions 
      float4   vStepTexel    : packoffset(c2);  // particle animation parameter: xy:1/animsubdiv, yw:1/texturesize
      float4   vConfig       : packoffset(c3);  // particle flags: x=animated, y=usernormals, z=distorted, w=z-offset
    #if !defined(PARTICLES_USE_HARDWARESPANNING) && defined(PARTICLES_GEOMETRY)
      float4   vParticleColor: packoffset(c4);  // Used for particle color for non-HWS geometry particles
    #else
      float4   vCornerUV     : packoffset(c4);  // Corners: xy:upper left, zw: lower right. E.g. -0.5,-0.5,0.5,0.5.
    #endif
      float4 BacklightScale_TessFactor  : packoffset(c5); // x: BacklightingScale, y: 1.0 / (1-BacklightScale)^2 zw: Tesselation factors (for domainshader frequency sampling)

    // dynamic lighting constants
    $if defined(LIGHTING_MOBILE)
      float3 ParticleEffectPosition : packoffset(c6);

      $if defined(LIGHTING_DYNAMIC)
        float4 SPHCoefficientsR : packoffset(c7);
        float4 SPHCoefficientsG : packoffset(c8);
        float4 SPHCoefficientsB : packoffset(c9);
      $endif
    $elif defined(LIGHTING_DYNAMIC)
      float4 DynamicLightConfig : packoffset(c6); // x: num directional lights, y: num point lights, z: num spot lights, w: empty
      float4x4 ShadowCastLightViewProjection : packoffset(c7);
      float4 ShadowFadeOutParameters_Size  : packoffset(c11);  // xy: FadeParameters zw: InvShadowMapSize
      DynamicLightDirectional DirectionalLights[MAX_NUM_DYNAMIC_LIGHTS_DIRECTIONAL] : packoffset(c12);
      DynamicLightPoint PointLights[MAX_NUM_DYNAMIC_LIGHTS_POINT] : packoffset(c14);
      DynamicLightSpot SpotLights[MAX_NUM_DYNAMIC_LIGHTS_SPOT]    : packoffset(c78);
    $endif
    }
  #endif



  #if defined(_IS_VERTEX_SHADER)
    #ifdef DESATURATION
      StructuredBuffer<uint2> DesaturationColorIn : register(t3);
    #endif //DESATURATION

    $if defined(LIGHTING_STATIC)
      cbuffer g_GlobalConstantBufferLightGrid : register (b3)
      {
      $if defined(LIGHTING_MOBILE)
        float4 AmbientColor : packoffset(c0);
        float4 DiffuseColor : packoffset(c1);
        float4 StaticLightDir : packoffset(c2);
      $else
        float4   Light0        : packoffset(c0);
        float4   Light1        : packoffset(c1);
        float4   Light2        : packoffset(c2);
        float4   Light3        : packoffset(c3);
        float4   Light4        : packoffset(c4);
        float4   Light5        : packoffset(c5);
      $endif
      }
    $endif
  #endif

  #ifdef _IS_PIXEL_SHADER
    Texture2D BaseTexture         : register(t0);
    sampler   BaseTextureSampler  : register(s0);
    Texture2D Normalmap           : register(t1);
    sampler   NormalmapSampler    : register(s1);
    Texture2D DepthTexture        : register(t2);
    sampler   DepthTextureSampler : register(s2);
  #endif

#else

  #ifdef _IS_VERTEX_SHADER
    float4x4 matMV         : register(c0);
    float4x4 matPJ         : register(c4);
    float4x4 matMVP        : register(c8);
    float4x4 matEye        : register(c12);
    float4x4 matInvEye     : register(c16);
    float4x4 matM2WS       : register(c20);
    float4 contextCP       : register(c24);
    float3 cameraPosition  : register(c31);   // worldspace camera position

    float4 fadeParams      : register(c38);

    // lightgrid lighting
    $if defined(LIGHTING_STATIC)
      $if defined(LIGHTING_MOBILE)
        float4 AmbientColor   : register(c32);
        float4 DiffuseColor   : register(c33);
        float4 StaticLightDir : register(c34);
      $else
        float4 Light0          : register(c32);
        float4 Light1          : register(c33);
        float4 Light2          : register(c34);
        float4 Light3          : register(c35);
        float4 Light4          : register(c36);
        float4 Light5          : register(c37);
      $endif
    $endif

    float4 vSubDivAspect   : register(c39);
    float4 vStepTexel      : register(c40);
    float4 vConfig         : register(c41);
    #if !defined(PARTICLES_USE_HARDWARESPANNING) && defined(PARTICLES_GEOMETRY)
      float4 vParticleColor: register(c42);    // Used for particle color for non-HWS geometry particles
    #else
      float4 vCornerUV       : register(c42);  // Corners: xy:upper left, zw: lower right. E.g. -0.5,-0.5,0.5,0.5. Used for particle color for non-HWS geometry particles
    #endif 
      float4 BacklightScale_TessFactor  : register(c43); // x: BacklightingScale, y: 1.0 / (1-BacklightScale)^2 zw: Tesselation factors (for domainshader frequency sampling)

    #ifdef _VISION_XENON
      float4 vScreenSpanU    : register(c16); // world matrix
      float4 vScreenSpanV    : register(c17); // world matrix
      float4 vDepth          : register(c18); // world matrix
    #endif

    // dynamic lighting constants
    $if defined(LIGHTING_MOBILE)
      float3 ParticleEffectPosition : register(c44);

      $if defined(LIGHTING_DYNAMIC)
        float4 SPHCoefficientsR : register(c45);
        float4 SPHCoefficientsG : register(c46);
        float4 SPHCoefficientsB : register(c47);
      $endif
    $elif defined(LIGHTING_DYNAMIC)
      float4 DynamicLightConfig : register(c44); // x: num directional lights, y: num point lights, z: num spot lights, w: empty
      float4x4 ShadowCastLightViewProjection : register(c45);
      float4 ShadowFadeOutParameters_Size  : register(c49);  // xy: FadeParameters zw: InvShadowMapSize
      DynamicLightDirectional DirectionalLights[MAX_NUM_DYNAMIC_LIGHTS_DIRECTIONAL] : register(c50);
      DynamicLightPoint PointLights[MAX_NUM_DYNAMIC_LIGHTS_POINT] : register(c52);
      DynamicLightSpot SpotLights[MAX_NUM_DYNAMIC_LIGHTS_SPOT]    : register(c116);
    $endif

  #endif // _IS_VERTEX_SHADER

  #ifdef _IS_PIXEL_SHADER
    float3 gAmbientCol     : register(c21);

    sampler2D BaseTexture  : register(s0);
    sampler2D Normalmap    : register(s1);
    sampler2D DepthTexture : register(s2);
  #endif // _IS_PIXEL_SHADER
  
#endif

// vertex out - same for all platforms. Also used by ps_main
struct VS_OUT
{
  float4 ProjPos  : SV_Position;
  #ifdef PARTICLES_NORMAL_FROM_DIFF_ALPHA
    float4 UV : TEXCOORD0;  // zw: direction to border
  #else
    float2 UV : TEXCOORD0;
  #endif
    
  #ifdef _VISION_XENON
    float4 Color    : COLOR0_center;
  #else  
    float4 Color    : COLOR;
  #endif

  $if defined(SMOOTHANIM)
    float3 UVBlend : TEXCOORD1; // encodes UV and blend factor
  $endif

  $ifdef SOFTPARTICLES
    float4 SoftParams : TEXCOORD2; // xy=proj coords, z=depth, w=farclip/radius
  $endif

  #ifdef PARTICLES_USE_PROJECTION
    float2 UVProj : TEXCOORD3;
  #endif

  $if !defined(LIGHTING_MOBILE) && (defined(LIGHTING_STATIC) || defined(LIGHTING_DYNAMIC))
    float4 ColorBasis0  : TEXCOORD3;  // contains backlight.r in a
    float4 ColorBasis1  : TEXCOORD4;  // contains backlight.g in a
    float4 ColorBasis2  : TEXCOORD5;  // contains backlight.b in a
  $endif

  #ifdef PARTICLES_DEFERRED_OUTPUT
    float4 BiTangent : TEXCOORD6; // w=depth
    float3 Tangent : TEXCOORD7;

  #elif defined(_IS_VERTEX_SHADER) || defined(_IS_HULL_SHADER)
    $if defined(LIGHTING_DOMAINFREQ)
      float3 WorldPos : TEXCOORD6;
      float3 BiTangent: TEXCOORD7;
      float3 Tangent  : TEXCOORD8;
    $endif
  #endif

  #ifdef PARTICLES_USE_EYE_DIR
    float3 EyeDir : TEXCOORD8;
  #endif

  #ifdef PARTICLES_USE_PSCLIPPING
    float ClipValue : TEXCOORD7;
  #endif
};


$if defined(LIGHTING_DYNAMIC) || defined(LIGHTING_STATIC)
  #include <Shaders/ParticlesLighting.inc>
$endif


#ifdef _IS_VERTEX_SHADER

  /////////////////////////////////////////////////////////////////////////////////
  // this function uses the projection matrix to map the [-1..1] vertex position 
  // to frame buffer texture coordinates
  float2 GetProjectedCoords(float4 projPos)
  {
    float2 aligned = (projPos.xy / projPos.w);
    #if defined(_VISION_GLES2)
      return aligned * 0.5 + 0.5;
    #else
      return aligned * float2(0.5, -0.5) + 0.5;
    #endif
  }


  // calculate particle fading value
  float GetFadeScalar(float z)
  {
    return saturate(1.0-(z-fadeParams.x)*fadeParams.z);
  }

  float3 ApplyCameraOffset(float3 worldSpacePos, float depthOffsetFactor)
  {
    float3 camDir = cameraPosition-worldSpacePos;
    return worldSpacePos + depthOffsetFactor*camDir; // percentage fraction
  }

  #ifndef PARTICLES_CUSTOM_UV_SWIZZLE
    #define SwizzleUV(_in) _in
  #endif



  // calculate tile UV for given frame index
  float2 GetAnimFrameUV(float fFrame, float2 corner)
  {
    #if defined(_VISION_PSP2)
      float t = fFrame / vSubDivAspect.x;
      float2 v1 = float2(frac(t) * vSubDivAspect.x, floor(t));
    #else
      float2 v1;
      v1.x = modf(fFrame / vSubDivAspect.x, v1.y) * vSubDivAspect.x;
    #endif
    
    v1 *= vStepTexel.xy;
    float2 v2 = float2(v1.x+vStepTexel.x-vStepTexel.z, v1.y+vStepTexel.y-vStepTexel.w);
    v1 += vStepTexel.zw;
    float2 res = float2(
      corner.x * v2.x + (1.0f-corner.x) * v1.x,
      corner.y * v2.y + (1.0f-corner.y) * v1.y);
    
    return res;
  }

  #ifdef PARTICLES_GEOMETRY
  
    // the following define can be used in custom code do define a custom MatrixFromAxisAngle function to return a custom 3x3 rotation matrix for geometry particles.
    // Function arguments are the rotation axis and the rotation angle (rad) around it
    #ifndef PARTICLES_GEOMETRY_CUSTOM_ROTATION_MATRIX
    
      // used for geometry rotation:
      float3x3 MatrixFromAxisAngle(float3 dir, float fAngle)
      {
        float s, c;
        float xx, yy, zz, xy, yz, zx, xs, ys, zs, one_c;

        sincos(fAngle, s,c);

        xx = dir.x * dir.x;
        yy = dir.y * dir.y;
        zz = dir.z * dir.z;
        xy = dir.x * dir.y;
        yz = dir.y * dir.z;
        zx = dir.z * dir.x;
        xs = dir.x * s;
        ys = dir.y * s;
        zs = dir.z * s;
        one_c = 1.0f - c;

        float3x3 m;
        m._m00 = (one_c * xx) + c;
        m._m01 = (one_c * xy) - zs;
        m._m02 = (one_c * zx) + ys;

        m._m10 = (one_c * xy) + zs;
        m._m11 = (one_c * yy) + c;
        m._m12 = (one_c * yz) - xs;
        
        m._m20 = (one_c * zx) - ys;
        m._m21 = (one_c * yz) + xs;
        m._m22 = (one_c * zz) + c;
        return m;
      }

    #endif // ifndef PARTICLES_GEOMETRY_CUSTOM_ROTATION_MATRIX
  #endif // PARTICLES_GEOMETRY


  #ifdef PARTICLES_USE_HARDWARESPANNING

    #if defined(_VISION_PS3) || defined(_VISION_PSP2)

      struct VS_IN
      {
        #ifdef PARTICLES_GEOMETRY
          float3 MeshPosition : POSITION;   // mesh data
          float4 Color        : COLOR;
          float2 UV0           : TEXCOORD0; // mesh data
          float4 ObjPosAndSize : TEXCOORD3; // center.xyz, w=size
          float3 Distortion    : TEXCOORD4; // x=angle
          float3 RotationAxis  : TEXCOORD5; // Rotation
          float4 AnimFrame     : TEXCOORD6; // x=frame#1, y=frame#2, z=weight
        #else // billboards:
          float4 ObjPosAndSize : POSITION;
          float3 Normal : NORMAL;
          float4 Color    : COLOR;
          float3 Distortion : TEXCOORD0;
          float3 AnimFrame : TEXCOORD1;
          float2 Corners : TEXCOORD2;
        #endif
      };

    #else

      struct VS_IN
      {
        #ifdef PARTICLES_GEOMETRY
          float3 MeshPosition : POSITION;   // mesh data
          
          #ifdef _VISION_XENON
            float4 Color    : COLOR0_center;
          #else  
            float4 Color    : COLOR;
          #endif
          float2 UV0           : TEXCOORD0; // mesh data
          float4 ObjPosAndSize : TEXCOORD3; // center.xyz, w=size
          float3 Distortion    : TEXCOORD4; // x=angle
          float3 RotationAxis  : TEXCOORD5; // Rotation
          float4 AnimFrame     : TEXCOORD6; // x=frame#1, y=frame#2, z=weight
        #else // billboards:
          float2 Corners : POSITION;
          float3 Normal : NORMAL;
          #ifdef _VISION_XENON
            float4 Color    : COLOR0_center;
          #else  
            float4 Color    : COLOR;
          #endif
          float3 Distortion : TEXCOORD0;
          float4 AnimFrame : TEXCOORD1; // x=frame#1, y=frame#2, z=weight
          float4 ObjPosAndSize : TEXCOORD2; // center.xyz, w=size

          #ifdef DESATURATION
            uint InstanceID        : SV_InstanceID;
          #endif //DESATURATION
        #endif
      };

    #endif


    ///////////////////////////////////////////////////////////////////////////////////////////////
    // All platforms
    ///////////////////////////////////////////////////////////////////////////////////////////////

    #if defined(_VISION_XENON)
      VS_OUT vs_main( int i : INDEX )
    #else
      VS_OUT vs_main( VS_IN In )
    #endif
    {
      VS_OUT Out;
      
      // setup aliases used in the shader:
      float4 ObjPosAndSize;
      float2 Corner;
      float4 Color;
      float4 Distortion;
      float4 Normal;
      float4 AnimFrame; // x=frame#1, y=frame#2, z=weight
      float4 RotationAxis;
        
      #if defined(_VISION_XENON)
        #ifdef PARTICLES_GEOMETRY
          float3 MeshPosition;
          // Need some constructive casting so 360 compiler doesn't optimize incorrectly
          int iVertexCount = (int)vCornerUV.x;
          int iParticleIndex = (int)( (i+0.5f) / iVertexCount );
          int iGeometryVertexIndex = i - (iParticleIndex * iVertexCount);
          asm
          {
            // Fetch the Mesh Geometry Vertex Data
            vfetch MeshPosition.xyz, iGeometryVertexIndex, position0;
            vfetch Corner.xy, iGeometryVertexIndex, texcoord0;

            // Fetch the Instance/Particle Data 
            vfetch ObjPosAndSize, iParticleIndex, texcoord3;
            vfetch Distortion, iParticleIndex, texcoord4;
            vfetch RotationAxis.xyz, iParticleIndex, texcoord5;
            vfetch AnimFrame, iParticleIndex, texcoord6;
            vfetch Color, iParticleIndex, color0;
            vfetch Normal, iParticleIndex, normal; // TODO: Should normal come from geometry in this case?
          };
        #else
          float2 Corners[4];
          Corners[0].x = 0.0f; Corners[0].y = 1.0f;
          Corners[1].x = 1.0f; Corners[1].y = 1.0f;
          Corners[2].x = 1.0f; Corners[2].y = 0.0f;
          Corners[3].x = 0.0f; Corners[3].y = 0.0f;

          int iDiv = i / 4;
          int iCornerIndex = i - (iDiv * 4);

          // Fetch the particle data using iDiv.
          asm
          {
            vfetch ObjPosAndSize, iDiv, position0;
            vfetch Distortion, iDiv, texcoord0;
            vfetch AnimFrame, iDiv, texcoord1;
            vfetch Color, iDiv, color0;
            vfetch Normal, iDiv, normal;
          };

          Corner = Corners[iCornerIndex].xy;
        #endif
      #else
        // default setup:
        #ifdef PARTICLES_GEOMETRY
          float3 MeshPosition = In.MeshPosition;
          ObjPosAndSize = In.ObjPosAndSize;
          Color = In.Color;
          Distortion.xyz = In.Distortion;
          AnimFrame.xyz = In.AnimFrame.xyz;
          Corner = In.UV0;
          RotationAxis.xyz = In.RotationAxis;
        #else
          ObjPosAndSize = In.ObjPosAndSize;
          Corner = In.Corners;
          Color = In.Color;
          Distortion.xyz = In.Distortion;
          Normal.xyz = In.Normal;
          AnimFrame.xyz = In.AnimFrame.xyz;
        #endif // PARTICLES_GEOMETRY
      #endif
        
      
      
      // "vDepth" is the negated camera view direction in worldspace
      float4 vDepth = matInvEye._m02_m12_m22_m32;
      
      #ifdef PARTICLES_CUSTOM_SPAN_VECTORS
        float3 vScreenSpanV = CustomSpanVectorV; // use defines in custom code
        float3 vScreenSpanU = CustomSpanVectorU;
      #elif defined (PARTICLES_ALWAYS_UP)
        float3 vScreenSpanV = float3(0,0,1); // up vector
        float3 vScreenSpanU = normalize(cross(vScreenSpanV,vDepth.xyz));
      #elif defined (PARTICLES_SPAN_DIRECTION)
        float3 vPosDir = ObjPosAndSize.xyz - cameraPosition.xyz;
        float3 vScreenSpanV = matInvEye._m01_m11_m21_m31; // start spanning with camera up
        float3 vScreenSpanU = normalize(cross(vScreenSpanV,vPosDir.xyz));
        vScreenSpanV = normalize(cross(vPosDir.xyz,vScreenSpanU));
      #else
        float4 vScreenSpanU = matInvEye._m00_m10_m20_m30;
        float4 vScreenSpanV = matInvEye._m01_m11_m21_m31;
      #endif

      // Copy Color
      Out.Color = Color;

      // Compute TexCoord
      if (vConfig.x > 0.0f)
      {
        Out.UV.xy = SwizzleUV(GetAnimFrameUV(AnimFrame.x*255.0f + 0.001f, Corner));
        $if defined(SMOOTHANIM)
          Out.UVBlend.xy = SwizzleUV(GetAnimFrameUV(AnimFrame.y*255.0f + 0.001f, Corner));
          Out.UVBlend.z = AnimFrame.z; // blend value
        $endif
      }
      else
      {
        Out.UV.xy = SwizzleUV(Corner.xy * float2(vSubDivAspect.w, 1.0f));
        $if defined(SMOOTHANIM)
          Out.UVBlend = 0;
        $endif
      }

      // Direction to border
      #ifdef PARTICLES_NORMAL_FROM_DIFF_ALPHA
        Out.UV.zw = Corner - float2(0.5f, 0.5f);
      #endif

      // Compute Position
      float3 vSpanVec;
      float4 vWorldPos;
      
      #ifdef PARTICLES_GEOMETRY
        float3x3 rotation = MatrixFromAxisAngle(RotationAxis.xyz, Distortion.x);
        vWorldPos.xyz = mul(rotation, MeshPosition) * ObjPosAndSize.w + ObjPosAndSize.xyz;
        vWorldPos.w = 1.0f;
      #else // PARTICLES_GEOMETRY:

        $ifdef LOCALSPACE
          float3 vNormal = (vConfig.y > 0.0f) ? mul((float3x3)matM2WS, Normal.xyz) : vDepth.xyz;
        $else
          float3 vNormal = (vConfig.y > 0.0f) ? Normal.xyz : vDepth.xyz;
        $endif
      
        // Corner.xy is in the local particle space (relative to its "center")
        float2 vSpanFactors = lerp(vCornerUV.xy,vCornerUV.zw,Corner.xy);
         
        float3 biTangent;
        float3 tangent;

        if (vConfig.z > 0.0f)   // use distortion
        {
          $ifdef LOCALSPACE
            vWorldPos.xyz = mul( matM2WS, float4(ObjPosAndSize.xyz,1.0) ).xyz;
            float3 vDistort = mul((float3x3)matM2WS, Distortion.xyz);
          $else
            vWorldPos.xyz = ObjPosAndSize.xyz;
            // "vDistort" is the vector from this particle to the next particle in the trail
            float3 vDistort = Distortion.xyz;
          $endif
          
          // Determine the extrude direction of the particle (in worldspace)
          #ifndef PARTICLES_TRAIL
            vSpanVec = normalize(cross(vDistort.xyz, vNormal));
          #else
            // Use the view direction from the camera to the particle to span the particle plane
            float3 vCameraToParticle = ObjPosAndSize.xyz - cameraPosition.xyz;
            float3 vParticleTravelDirection = vDistort.xyz;
            
            // In trail mode we need different corner vectors on the right half of the distorted particle. The simulation tracks this into the normal vector
            // "Normal" is the vector from this particle's next particle to that particle's next particle (it's used to "fix up" the right half of the vertices 
            // of the current particle to match the left half vertices of the next particle)
            if(Corner.x > 0.5)
            {
              // The view direction to the next particle is the view direction to the current particle plus the displacement from the current particle to the next.
              vCameraToParticle += vParticleTravelDirection.xyz;
              vParticleTravelDirection = Normal.xyz;
            }
            
            // The cross product will only fail if the view direction to the particle is collinear to the particle's travel direction.
            // This case is so rare that we ignore it to safe performance.
            vSpanVec = normalize(cross(vParticleTravelDirection, vCameraToParticle));
          #endif

          biTangent = -vSpanVec; // It is still normalized at this point!
          tangent = normalize(vDistort);

          vSpanVec *= ObjPosAndSize.w;
          
          vWorldPos.xyz += vDistort * vSpanFactors.x - vSpanVec * vSpanFactors.y;
          vWorldPos.w = 1.0f;
        }
        else
        {
          float3 vUSpan0 = vScreenSpanU.xyz * ObjPosAndSize.w;
          float3 vVSpan0 = vScreenSpanV.xyz * ObjPosAndSize.w;
          
          // do the rotation (angle is in distortion, because distortion isn't needed here)
          float sw = sin(Distortion.x);
          float cw = cos(Distortion.x);
          float3 vUSpan = vUSpan0*cw + vVSpan0*sw;
          float3 vVSpan = vUSpan0*sw - vVSpan0*cw;
          
          $ifdef LOCALSPACE
            vWorldPos.xyz = mul( matM2WS, float4(ObjPosAndSize.xyz,1.0) ).xyz;
          $else
            vWorldPos.xyz = ObjPosAndSize.xyz;
          $endif

          vWorldPos.xyz += vUSpan * vSpanFactors.x - vVSpan * vSpanFactors.y;
          vWorldPos.w = 1.0f;

          biTangent = -normalize(vVSpan);
          tangent = normalize(vUSpan);
        }

      #endif // PARTICLES_GEOMETRY

      vWorldPos.xyz = ApplyCameraOffset(vWorldPos.xyz, vConfig.w);

      #ifdef PARTICLES_USE_EYE_DIR
        // Normalize in PS
        Out.EyeDir = cameraPosition-vWorldPos.xyz;
      #endif

      $ifdef LOCALSPACE
        float4 vESPos = mul( matEye, vWorldPos );
        Out.ProjPos = mul( matPJ, vESPos );
      $else
        // Transform Position                        
        Out.ProjPos = mul( matMVP, vWorldPos );
      $endif

      #ifdef PARTICLES_DEFERRED_OUTPUT
        Out.BiTangent.w = Out.ProjPos.z / contextCP.y; // depth
      #endif

      $ifdef SOFTPARTICLES
        Out.SoftParams.xy = GetProjectedCoords(Out.ProjPos);
        Out.SoftParams.z = mul( matMV, vWorldPos ).z * contextCP.z; // calculate linear depth value, which is compared againsted the linear depth from the texture in the pixel shader
        Out.SoftParams.w = contextCP.y / ObjPosAndSize.w;
      $endif

      #ifdef PARTICLES_USE_PROJECTION
        Out.UVProj = GetProjectedCoords(Out.ProjPos);
      #endif

      // fade
      Out.Color.a *= GetFadeScalar(Out.ProjPos.z);

      #ifdef DESATURATION
        #ifdef PARTICLES_GEOMETRY
          uint iID = 0;
        #else
          uint iID = In.InstanceID;
        #endif

        float3 cDesaturationColor = GetPointDesaturation(DesaturationColorIn, iID);
        float fDesaturationLuminance = cDesaturationColor.r * 0.299f + cDesaturationColor.g * 0.587f + cDesaturationColor.b * 0.114f;

        Out.Color.a *= fDesaturationLuminance;
      #endif //DESATURATION

      $if defined(LIGHTING_STATIC) || defined(LIGHTING_DYNAMIC)
        $if defined(LIGHTING_MOBILE)
          Out.Color.rgb *= ComputeLightingMobile(vWorldPos);
        $else
          Out.ColorBasis0 = float4(0.0f, 0.0f, 0.0f, 0.0f);
          Out.ColorBasis1 = float4(0.0f, 0.0f, 0.0f, 0.0f);
          Out.ColorBasis2 = float4(0.0f, 0.0f, 0.0f, 0.0f);
          float3 backlight = float3(0.0f, 0.0f, 0.0f);

          float3 worldSpaceNormal = cross(tangent, biTangent);
          float3 lightingBasisVectors_WorldSpace[3];
          ComputeLightingBasisWS(lightingBasisVectors_WorldSpace, tangent, biTangent, worldSpaceNormal);

          // Static lighting doesn't make any sense in the domain shader.
          $if defined(LIGHTING_STATIC)
            Out.ColorBasis0.rgb = GetLightGridDiffuse_WS(lightingBasisVectors_WorldSpace[0], Light0,Light1,Light2,Light3,Light4,Light5).rgb;
            Out.ColorBasis1.rgb = GetLightGridDiffuse_WS(lightingBasisVectors_WorldSpace[1], Light0,Light1,Light2,Light3,Light4,Light5).rgb;
            Out.ColorBasis2.rgb = GetLightGridDiffuse_WS(lightingBasisVectors_WorldSpace[2], Light0,Light1,Light2,Light3,Light4,Light5).rgb;
            backlight = GetLightGridDiffuse_WS(-worldSpaceNormal, Light0,Light1,Light2,Light3,Light4,Light5).rgb;
          $endif

          // Dynamic lighting however can be executed in domain shader to improve attention, light direction and shadow sampling
          $if !defined(LIGHTING_DOMAINFREQ) && defined(LIGHTING_DYNAMIC)
            PerformDynamicLighting(Out.ColorBasis0.rgb, Out.ColorBasis1.rgb, Out.ColorBasis2.rgb, backlight, 
                                    worldSpaceNormal, lightingBasisVectors_WorldSpace, vWorldPos);
          $endif

          
          $if !defined(LIGHTING_DOMAINFREQ) 
            backlight *= BacklightScale_TessFactor.x;  // Apply when all lighting is accumlated.
          $endif
          // Pack backlighting into existing color basis registers to reduce interpolator count.
  		    Out.ColorBasis0.a = backlight.r;
          Out.ColorBasis1.a = backlight.g;
          Out.ColorBasis2.a = backlight.b;

          // Need to store tangent space and world pos for domain shader.
          $if defined(LIGHTING_DOMAINFREQ) 
            Out.WorldPos = vWorldPos;
            Out.Tangent = tangent;
            Out.BiTangent = biTangent;
          $endif
        $endif
      $endif

      #ifdef PARTICLES_DEFERRED_OUTPUT
        Out.Tangent.xyz = tangent;
        Out.BiTangent.xyz = biTangent;
      #endif

      #if defined(MOBILE_NORMAL_TO_COLOR)
        Out.Color.rgb = ComputeMobileLightingNormal(vWorldPos);
      #elif defined(WORLDPOS_TO_COLOR)
        Out.Color.rgb = vWorldPos.xyz;
      #endif

      return Out;
    }

  #else // CPU spanning; same for all version

    struct VS_IN
    {
      float3 ObjPosAndSize  : POSITION;
      float4 Color          : COLOR;
      float2 UV             : TEXCOORD0;

      #ifdef DESATURATION
        uint VertexID       : SV_VertexID;
      #endif //DESATURATION
    };


    VS_OUT vs_main( VS_IN In )
    {
      VS_OUT Out = (VS_OUT)(0);
      float4 vWorldPos = float4(In.ObjPosAndSize.xyz, 1.0f);
      vWorldPos.xyz = ApplyCameraOffset(vWorldPos.xyz, vConfig.w);

      Out.ProjPos = mul(matMVP, vWorldPos);
      #ifdef PARTICLES_USE_PROJECTION
        Out.UVProj = GetProjectedCoords(Out.ProjPos);
      #endif
      Out.UV = SwizzleUV(In.UV);

      #if defined(PARTICLES_GEOMETRY)
        Out.Color = vParticleColor; 
      #else
        Out.Color = In.Color;
      #endif

      Out.Color.a *= GetFadeScalar(Out.ProjPos.z);

      #ifdef DESATURATION
        float3 cDesaturationColor = GetPointDesaturation(DesaturationColorIn, In.VertexID / 6);
        float fDesaturationLuminance = cDesaturationColor.r * 0.299f + cDesaturationColor.g * 0.587f + cDesaturationColor.b * 0.114f;

        Out.Color.a *= fDesaturationLuminance;
      #endif //DESATURATION

      // Static mobile lighting
      $if defined(LIGHTING_MOBILE)
        Out.Color.rgb *= ComputeLightingMobile(vWorldPos);
      $endif

      #if defined(MOBILE_NORMAL_TO_COLOR)
        Out.Color.rgb = ComputeMobileLightingNormal(vWorldPos);
      #elif defined(WORLDPOS_TO_COLOR)
        Out.Color.rgb = vWorldPos.xyz;
      #endif

      return Out;
    }

  #endif // PARTICLES_USE_HARDWARESPANNING
#endif //_IS_VERTEX_SHADER


#ifdef _IS_PIXEL_SHADER

  ///////////////////////////////////////////////////////////
  // pixel shader: same for all modes/platforms
  ///////////////////////////////////////////////////////////


  #ifdef PARTICLES_DEFERRED_OUTPUT
    PS_OUT ps_main( VS_OUT In )
  #else
    float4 ps_main( VS_OUT In ) : SV_Target
  #endif
  {
    #ifdef PARTICLES_USE_PSCLIPPING
      clip(In.ClipValue);
    #endif

    // Sample diffuse texture with or without smooth blending
    $if defined(SMOOTHANIM)
      float4 frame0 = vTex2D(BaseTexture, BaseTextureSampler, In.UV.xy);
      float4 frame1 = vTex2D(BaseTexture, BaseTextureSampler, In.UVBlend.xy);
      float4 texColor = lerp(frame0, frame1, In.UVBlend.z);
    $else
      float4 texColor = vTex2D(BaseTexture, BaseTextureSampler, In.UV.xy);
    $endif

    // General output color: diffuse*color
    float4 color = texColor * In.Color;

    $ifdef SOFTPARTICLES
      float fDepth = READ_CONVERTED_DEPTH (DepthTexture, DepthTextureSampler, In.SoftParams.xy).r;
      float fSoftFade = saturate((fDepth - In.SoftParams.z) * In.SoftParams.w);
      color.a *= fSoftFade;
    $endif

    #ifndef PIXEL_OUTPUT_OVERWRITE
      $ifdef ALPHATESTINSHADER
        clip(color.a - 1.0f / 255.0f);
      $endif
    #endif

    // Normalmap sampling is activated by one of the lighting or deferred output options.
    float3 normalTangentSpace = float3(0.5,0.5,0.5);
    $if !defined(LIGHTING_MOBILE)
      #if defined(PARTICLES_USE_NORMALMAP)
        $if defined(SMOOTHANIM)
          float3 frame0Normal = vTex2D(Normalmap, NormalmapSampler, In.UV.xy).xyz;
          float3 frame1Normal = vTex2D(Normalmap, NormalmapSampler, In.UVBlend.xy).xyz;
          normalTangentSpace = lerp(frame0Normal, frame1Normal, In.UVBlend.z);
        $else
          normalTangentSpace = vTex2D(Normalmap, NormalmapSampler, In.UV.xy).xyz;
        $endif 
        normalTangentSpace = normalTangentSpace * 2.0f - 1.0f;
      #elif defined(PARTICLES_NORMAL_FROM_DIFF_ALPHA)
        // Instead of using a normal map, a normal will be generated by interpreting the texColor as "curvature".
        normalTangentSpace = normalize(float3(In.UV.zw, texColor.a + 0.0001f));
      #endif
    $endif

 
    #ifdef PARTICLES_DEFERRED_OUTPUT
      // No normalizations because of performance to quality considerations
      float3 tangentSpaceUp = cross(In.Tangent, In.BiTangent.xyz);
      float3 normalWorldSpace = GetWorldNormal(float4(normalTangentSpace, 0.0f), In.Tangent, In.BiTangent.xyz, tangentSpaceUp); 

      PixelDataUnpacked pixelData;
      pixelData.vAccInitial = float3(0,0,0); // ambient
      pixelData.fDepth = In.BiTangent.w;
      pixelData.vNormal = normalWorldSpace;
      pixelData.vDiffColor = color.xyz;
      pixelData.vSpecColor = float3(0,0,0);
      pixelData.fSpecExp = 1.0;
      pixelData.fMaterial = 1.0;
      return packPixelValues(pixelData);
    #else
      float3 lightingColor = float3(0.0f, 0.0f, 0.0f);

      $if !defined(LIGHTING_MOBILE) && (defined(LIGHTING_STATIC) || defined(LIGHTING_DYNAMIC))
        // standard lighting
        lightingColor = EvaluateLighting(normalTangentSpace, In.ColorBasis0.rgb, In.ColorBasis1.rgb, In.ColorBasis2.rgb);

        // Backlighting
        float backlightingFactor = 1.0f - color.a;
        lightingColor += float3(In.ColorBasis0.a, In.ColorBasis1.a, In.ColorBasis2.a) * backlightingFactor;

        // Ambient lighting & combine
        $if !defined(LIGHTING_STATIC)
          lightingColor += gAmbientCol; // Ambient color is contained in the lightgrid!
        $endif
        color.rgb *= lightingColor;
      $endif

      #ifdef PIXEL_OUTPUT_OVERWRITE // Used in DebugShadingEffects
        #ifdef PARTICLES_USE_PROJECTION
          normalTangentSpace.xy = In.UVProj;
        #endif
        color = OutputOverwrite(In.Color, texColor, lightingColor, normalTangentSpace, In.UV.xy);  
        $ifdef ALPHATESTINSHADER
          clip(color.a - 1.0f / 255.0f);
        $endif
      #endif

      return color;
    #endif
  }

#endif // _IS_PIXEL_SHADER

/*-----------------------------------------------------------------------
 * Copyright (c) 2018, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/ //--------------------------------------------------------------------
#include "GLSLProgram.h"
#include "nvgl/contextwindow_gl.hpp"
#include "nvgl/extensions_gl.hpp"
#include "nvgl/profiler_gl.hpp"
#include "nvh/appwindowcamerainertia.hpp"
#include <imgui/backends/imgui_impl_gl.h>
#include <list>
#include <nvpwindow.hpp>

//
// Camera animation: captured using '1' in the sample. Then copy and paste...
//
struct CameraAnim
{
  vec3f eye, focus;
};
static CameraAnim s_cameraAnim[] = {
    {vec3f(-1.26, 0.47, 1.01), vec3f(-0.96, 0.47, 0.00)},
    {vec3f(0.38, 0.34, 0.43), vec3f(0.37, 0.35, 0.14)},
    {vec3f(1.58, 0.54, 0.65), vec3f(0.65, 0.56, -0.06)},
    {vec3f(1.07, -1.91, 4.75), vec3f(0.07, -0.50, 0.22)},
    {vec3f(-3.41, 0.39, 2.76), vec3f(-0.96, -0.33, -0.28)},
    {vec3f(-0.94, -0.15, 0.69), vec3f(-0.96, -0.33, -0.28)},
    {vec3f(-0.25, -0.18, 0.67), vec3f(-0.27, -0.36, -0.29)},
    {vec3f(0.79, -0.26, 0.66), vec3f(0.77, -0.43, -0.30)},
    {vec3f(0.38, -0.98, 0.79), vec3f(0.36, -1.15, -0.16)},
    {vec3f(-1.46, -1.48, 0.05), vec3f(-0.58, -1.12, -0.41)},
    {vec3f(-1.64, -0.70, 0.30), vec3f(-0.76, -0.35, -0.16)},
    {vec3f(-1.13, -0.66, 1.39), vec3f(-0.76, -0.35, -0.16)},
    {vec3f(-0.20, -0.65, 3.05), vec3f(-0.20, -0.45, -0.11)},
    {vec3f(0.00, 0.00, 3.00), vec3f(0.00, 0.00, 0.00)},  //14 items
};
static int s_cameraAnimItem  = 0;
static int s_cameraAnimItems = 14;
#define ANIMINTERVALL 1.5f
static float s_cameraAnimIntervals = ANIMINTERVALL;
static bool  s_bCameraAnim         = true;
static bool  s_helpText            = false;

static nvgl::ProfilerGL s_profiler;
static double           s_statsCpuTime = 0;
static double           s_statsGpuTime = 0;

#define PROFILE_SECTION(name) nvgl::ProfilerGL::Section _tempTimer(s_profiler, name);

//-----------------------------------------------------------------------------
// Derive the Window for this sample
//-----------------------------------------------------------------------------
class MyWindow : public AppWindowCameraInertia
{
  bool m_validated;

public:
  ImGuiH::Registry    m_guiRegistry;
  nvgl::ContextWindow m_contextWindowGL;

  MyWindow()
      : m_validated(false)
  {
  }

  bool open(int posX, int posY, int width, int height, const char* title, const nvgl::ContextWindowCreateInfo& context);
  void renderScene();
  void processUI(int width, int height, double dt);

  virtual void onWindowClose() override;
  virtual void onWindowResize(int w, int h) override;
  //virtual void motion(int x, int y) override;
  //virtual void mousewheel(short delta) override;
  //virtual void mouse(NVPWindow::MouseButton button, ButtonAction action, int mods, int x, int y) override;
  //virtual void menu(int m) override;
  virtual void onKeyboard(MyWindow::KeyCode key, ButtonAction action, int mods, int x, int y) override;
  virtual void onKeyboardChar(unsigned char key, int mods, int x, int y) override;
  //virtual void idle() override;
  virtual void onWindowRefresh() override;
};

/////////////////////////////////////////////////////////////////////////
// Cst color
static const char* g_glslv_WVP_Position =
    "#version 330\n"
    "#extension GL_ARB_separate_shader_objects : enable\n"
    "uniform mat4 mWVP;\n"
    "layout(location=0) in  vec3 P;\n"
    "out gl_PerVertex {\n"
    "    vec4  gl_Position;\n"
    "};\n"
    "void main() {\n"
    "   gl_Position = mWVP * vec4(P, 1.0);\n"
    "}\n";
static const char* g_glslf_OneMinusCMYK_A =
    "#version 330\n"
    "#extension GL_ARB_separate_shader_objects : enable\n"
    "uniform vec4 CMYK;\n"
    "uniform float alpha;\n"
    "layout(location=0) out vec4 outCMYA;\n"
    "layout(location=1) out vec4 outKA;\n"
    "void main() {\n"
    "   outCMYA = vec4(1.0-CMYK.xyz,   alpha);\n"
    "   outKA   = vec4(1.0-CMYK.w,1,1, alpha);\n"
    "}\n";
static const char* g_glslf_RGBA =
    "#version 330\n"
    "#extension GL_ARB_separate_shader_objects : enable\n"
    "uniform vec4 RGBA;\n"
    "layout(location=0) out vec4 outRGBA;\n"
    "void main() {\n"
    "   outRGBA = RGBA;\n"
    "}\n";

/////////////////////////////////////////////////////////////////////////
// FBO resolve

static const char* g_glslv_Tc =
    "#version 330\n"
    "#extension GL_ARB_separate_shader_objects : enable\n"
    "uniform ivec2 viewportSz;\n"
    "layout(location=0) in  ivec2 P;\n"
    "layout(location=0) out vec2 TcOut;\n"
    "out gl_PerVertex {\n"
    "    vec4  gl_Position;\n"
    "};\n"
    "void main() {\n"
    "   TcOut = vec2(P);\n"
    "   gl_Position = vec4(vec2(P)/vec2(viewportSz)*2.0 - 1.0, 0.0, 1.0);\n"
    "}\n";

#define CMYK2RGB                                                                                                       \
  "vec3 OneMinusCMYK2RGB(in vec4 CMYK) {\n"                                                                            \
  "   vec3 cmy = vec3(1-min(1.0, (CMYK.x)+(CMYK.w)),\n"                                                                \
  "                   1-min(1.0, (CMYK.y)+(CMYK.w)),\n"                                                                \
  "                   1-min(1.0, (CMYK.z)+(CMYK.w)) );\n"                                                              \
  "   return cmy;\n"                                                                                                   \
  "}\n"
inline vec3f convertCMYK2RGB(vec4f CMYK)
{
  return vec3f(1 - nv_min(1.0f, (CMYK.x) + (CMYK.w)), 1 - nv_min(1.0f, (CMYK.y) + (CMYK.w)),
               1 - nv_min(1.0f, (CMYK.z) + (CMYK.w)));
}


// sampling 2 textures and output RGBA
static const char* g_glslf_Tex_CMYA_KA_2_RGBA =
    "#version 330\n"
    "#extension GL_ARB_separate_shader_objects : enable\n"
    "uniform vec4 CMYK_Mask;\n"
    "uniform sampler2D sampler_CMYA;\n"
    "uniform sampler2D sampler_KA;\n"
    "layout(location=0) in vec2 Tc;\n"
    "layout(location=0) out vec4 outColor;\n" CMYK2RGB
    "void main() {\n"
    "   vec4 c = vec4(0);\n"
    "   float alpha = 0;\n"
    "   c = texelFetch(sampler_CMYA, ivec2(Tc), 0);\n"
    "   alpha = c.w;\n"
    "   c.w = texelFetch(sampler_KA, ivec2(Tc), 0).x;\n"
    "   outColor = vec4(OneMinusCMYK2RGB(CMYK_Mask * (1-c)), alpha);\n"
    "}\n";

// for sampling MSAA Texture
#define DEFINE_GLSLF_TEXMS_CMYA_KA_2_RGBA(VAR, msaa)                                                                   \
  static const char* VAR =                                                                                             \
      "#version 330\n"                                                                                                 \
      "#extension GL_ARB_separate_shader_objects : enable\n"                                                           \
      "uniform vec4 CMYK_Mask;\n"                                                                                      \
      "uniform sampler2DMS samplerMS_CMYA;\n"                                                                          \
      "uniform sampler2DMS samplerMS_KA;\n"                                                                            \
      "layout(location=0) in vec2 Tc;\n"                                                                               \
      "layout(location=0) out vec4 outColor;\n" CMYK2RGB                                                               \
      "void main() {\n"                                                                                                \
      "   vec4 c = vec4(0);\n"                                                                                         \
      "   float alpha = 0;\n"                                                                                          \
      "   for(int i=0; i<" #msaa                                                                                       \
      "; i++) {\n"                                                                                                     \
      "       vec4 tex;\n"                                                                                             \
      "       tex = texelFetch(samplerMS_CMYA, ivec2(Tc), i);\n"                                                       \
      "       alpha += tex.w;"                                                                                         \
      "       tex.w = texelFetch(samplerMS_KA, ivec2(Tc), i).x;\n"                                                     \
      "       c += tex;"                                                                                               \
      "   }\n"                                                                                                         \
      "   outColor = vec4(OneMinusCMYK2RGB(CMYK_Mask * (1-(c / " #msaa "))), alpha/" #msaa                             \
      ");\n"                                                                                                           \
      "}\n";

DEFINE_GLSLF_TEXMS_CMYA_KA_2_RGBA(g_glslf_TexMS_CMYA_KA_2_RGBA_2x, 2)
DEFINE_GLSLF_TEXMS_CMYA_KA_2_RGBA(g_glslf_TexMS_CMYA_KA_2_RGBA_8x, 8)
DEFINE_GLSLF_TEXMS_CMYA_KA_2_RGBA(g_glslf_TexMS_CMYA_KA_2_RGBA_16x, 16)
static const char* g_glslf_TexMS_CMYA_KA_2_RGBA[3] = {g_glslf_TexMS_CMYA_KA_2_RGBA_2x, g_glslf_TexMS_CMYA_KA_2_RGBA_8x,
                                                      g_glslf_TexMS_CMYA_KA_2_RGBA_16x};

// for sampling MSAA Texture
#define DEFINE_GLSLF_IMAGEMS_CMYA_KA_2_RGBA(VAR, msaa)                                                                 \
  static const char* VAR =                                                                                             \
      "#version 420\n"                                                                                                 \
      "uniform vec4 CMYK_Mask;\n"                                                                                      \
      "uniform layout(rgba8) image2DMS imageMS_CMYA;\n"                                                                \
      "uniform layout(rgba8) image2DMS imageMS_KA;\n"                                                                  \
      "layout(location=0) in vec2 Tc;\n"                                                                               \
      "layout(location=0) out vec4 outColor;\n" CMYK2RGB                                                               \
      "void main() {\n"                                                                                                \
      "   vec4 c = vec4(0);\n"                                                                                         \
      "   float alpha = 0;\n"                                                                                          \
      "   for(int i=0; i<" #msaa                                                                                       \
      "; i++) {\n"                                                                                                     \
      "       vec4 tex;\n"                                                                                             \
      "       tex = imageLoad(imageMS_CMYA, ivec2(Tc), i);\n"                                                          \
      "       alpha += tex.w;"                                                                                         \
      "       tex.w = imageLoad(imageMS_KA, ivec2(Tc), i).x;\n"                                                        \
      "       c += tex;"                                                                                               \
      "   }\n"                                                                                                         \
      "   outColor = vec4(OneMinusCMYK2RGB(CMYK_Mask * (1-(c / " #msaa "))), alpha/" #msaa                             \
      ");\n"                                                                                                           \
      "}\n";

DEFINE_GLSLF_IMAGEMS_CMYA_KA_2_RGBA(g_glslf_ImageMS_CMYA_KA_2_RGBA_2x, 2)
DEFINE_GLSLF_IMAGEMS_CMYA_KA_2_RGBA(g_glslf_ImageMS_CMYA_KA_2_RGBA_8x, 8)
DEFINE_GLSLF_IMAGEMS_CMYA_KA_2_RGBA(g_glslf_ImageMS_CMYA_KA_2_RGBA_16x, 16)
static const char* g_glslf_ImageMS_CMYA_KA_2_RGBA[3] = {g_glslf_ImageMS_CMYA_KA_2_RGBA_2x, g_glslf_ImageMS_CMYA_KA_2_RGBA_8x,
                                                        g_glslf_ImageMS_CMYA_KA_2_RGBA_16x};

static GLenum drawBuffers[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3,
                               GL_COLOR_ATTACHMENT4, GL_COLOR_ATTACHMENT5, GL_COLOR_ATTACHMENT6};

//------------------------------------
// Blending Equations
#define BLENDINGLIST()                                                                                                 \
  ENUMDECL(GL_FUNC_ADD)                                                                                                \
  ENUMDECL(GL_SRC_NV)                                                                                                  \
  ENUMDECL(GL_DST_NV)                                                                                                  \
  ENUMDECL(GL_SRC_OVER_NV)                                                                                             \
  ENUMDECL(GL_DST_OVER_NV)                                                                                             \
  ENUMDECL(GL_SRC_IN_NV)                                                                                               \
  ENUMDECL(GL_DST_IN_NV)                                                                                               \
  ENUMDECL(GL_SRC_OUT_NV)                                                                                              \
  ENUMDECL(GL_DST_OUT_NV)                                                                                              \
  ENUMDECL(GL_SRC_ATOP_NV)                                                                                             \
  ENUMDECL(GL_DST_ATOP_NV)                                                                                             \
  /*ENUMDECL(GL_XOR_NV)*/                                                                                              \
  ENUMDECL(GL_MULTIPLY_NV)                                                                                             \
  ENUMDECL(GL_SCREEN_NV)                                                                                               \
  ENUMDECL(GL_OVERLAY_NV)                                                                                              \
  ENUMDECL(GL_DARKEN_NV)                                                                                               \
  ENUMDECL(GL_LIGHTEN_NV)                                                                                              \
  ENUMDECL(GL_COLORDODGE_NV)                                                                                           \
  ENUMDECL(GL_COLORBURN_NV)                                                                                            \
  ENUMDECL(GL_HARDLIGHT_NV)                                                                                            \
  ENUMDECL(GL_SOFTLIGHT_NV)                                                                                            \
  ENUMDECL(GL_DIFFERENCE_NV)                                                                                           \
  ENUMDECL(GL_EXCLUSION_NV)                                                                                            \
  /*ENUMDECL(INVERT)*/                                                                                                 \
  ENUMDECL(GL_INVERT_RGB_NV)                                                                                           \
  ENUMDECL(GL_LINEARDODGE_NV)                                                                                          \
  ENUMDECL(GL_LINEARBURN_NV)                                                                                           \
  ENUMDECL(GL_VIVIDLIGHT_NV)                                                                                           \
  ENUMDECL(GL_LINEARLIGHT_NV)                                                                                          \
  ENUMDECL(GL_PINLIGHT_NV)                                                                                             \
  ENUMDECL(GL_HARDMIX_NV)                                                                                              \
  /*ENUMDECL(GL_HSL_HUE_NV) can only work with RGB*/                                                                   \
  /*ENUMDECL(GL_HSL_SATURATION_NV) can only work with RGB*/                                                            \
  /*ENUMDECL(GL_HSL_COLOR_NV) can only work with RGB*/                                                                 \
  /*ENUMDECL(GL_HSL_LUMINOSITY_NV) can only work with RGB*/                                                            \
  ENUMDECL(GL_PLUS_NV)                                                                                                 \
  ENUMDECL(GL_PLUS_CLAMPED_NV)                                                                                         \
  ENUMDECL(GL_PLUS_CLAMPED_ALPHA_NV)                                                                                   \
  ENUMDECL(GL_PLUS_DARKER_NV)                                                                                          \
  ENUMDECL(GL_MINUS_NV)                                                                                                \
  ENUMDECL(GL_MINUS_CLAMPED_NV)                                                                                        \
  ENUMDECL(GL_CONTRAST_NV)                                                                                             \
  ENUMDECL(GL_INVERT_OVG_NV)                                                                                           \
  /*ENUMDECL(GL_RED_NV)*/                                                                                              \
  /*ENUMDECL(GL_GREEN_NV)*/                                                                                            \
  /*ENUMDECL(GL_BLUE_NV)*/                                                                                             \
  ENUMDECL(GL_ZERO)

#define ENUMDECL(a) #a,
const char* blendequationNames[] = {BLENDINGLIST()};
#undef ENUMDECL
#define ENUMDECL(a) a,
GLenum blendequations[]   = {BLENDINGLIST()};
int    g_curBlendEquation = 0;

//------------------------------------
// Blend funcs
#define BLENDINGFUNCS()                                                                                                \
  ENUMDECL(GL_ONE)                                                                                                     \
  ENUMDECL(GL_SRC_COLOR)                                                                                               \
  ENUMDECL(GL_ONE_MINUS_SRC_COLOR)                                                                                     \
  ENUMDECL(GL_DST_COLOR)                                                                                               \
  ENUMDECL(GL_ONE_MINUS_DST_COLOR)                                                                                     \
  ENUMDECL(GL_SRC_ALPHA)                                                                                               \
  ENUMDECL(GL_ONE_MINUS_SRC_ALPHA)                                                                                     \
  ENUMDECL(GL_DST_ALPHA)                                                                                               \
  ENUMDECL(GL_ONE_MINUS_DST_ALPHA)                                                                                     \
  ENUMDECL(GL_CONSTANT_COLOR)                                                                                          \
  ENUMDECL(GL_ONE_MINUS_CONSTANT_COLOR)                                                                                \
  ENUMDECL(GL_CONSTANT_ALPHA)                                                                                          \
  ENUMDECL(GL_ONE_MINUS_CONSTANT_ALPHA)                                                                                \
  ENUMDECL(GL_ZERO)

#undef ENUMDECL
#define ENUMDECL(a) #a,
const char* blendfuncNames[] = {BLENDINGFUNCS()};
#undef ENUMDECL
#define ENUMDECL(a) a,
GLenum blendfuncs[] = {BLENDINGFUNCS()};

int g_blendSRC = 5;
int g_blendDST = 6;
//------------------------------------
// used to give a string name to the class through constructor: helps for error checking
#define P(p) p(#p)

GLSLProgram P(g_prog_Cst_OneMinusCMYK_A);
GLSLProgram P(g_progPR_Cst_OneMinusCMYK_A);
GLSLProgram P(g_prog_Cst_RGBA);
GLSLProgram P(g_progPR_Cst_RGBA);

GLSLProgram P(g_progTexCMYA_KA_2_RGBA);
GLSLProgram g_progTexMS_CMYA_KA_2_RGBA[3];
GLSLProgram P(g_progTex_CMYA_KA_2_RGBA);
GLSLProgram g_progImageMS_CMYA_KA_2_RGBA[3];

GLuint g_vboCircle                           = 0;
GLuint g_vboQuad                             = 0;
float  g_alpha                               = 0.5;
int    g_NObjs                               = 6;
bool   g_usePathObj                          = true;
bool   g_activeC                             = true;
bool   g_activeM                             = true;
bool   g_activeY                             = true;
bool   g_activeK                             = true;
bool   g_blendEnable                         = true;
int    g_MSAARaster                          = 8;
int    g_MSAAVal[]                           = {1, 2, 8, 16};
int    g_CurMSAAColor                        = 0;
bool   g_has_GL_NV_framebuffer_mixed_samples = false;
bool   g_buseUI                              = true;


unsigned int g_pathObj = 0;

// From the driver, renderbuffers are really useless: they are just textures that cannot be used as textures
// it is now advised to just use textures and avoid renderbuffers...
//#define USE_RENDERBUFFERS

// FBO Stuff
GLuint fboSz[2] = {0, 0};
namespace Texture {
GLuint CMYA;
GLuint KA;
GLuint MS_CMYA;
GLuint MS_KA;
#ifndef USE_RENDERBUFFERS
GLuint DST;
GLuint MS_DST;
#endif
};  // namespace Texture

#ifdef USE_RENDERBUFFERS
namespace Renderbuffer {
GLuint RGBA;
GLuint RGBAMS;
GLuint DST;
GLuint DSTMS;
};  // namespace Renderbuffer
#endif

namespace FBO {
GLuint TexMS_CMYA_KA_DST;
GLuint Tex_CMYA_KA;
GLuint TexMS_CMYA;
GLuint Tex_CMYA;
GLuint TexMS_KA;
GLuint Tex_KA;
#ifdef USE_RENDERBUFFERS
GLuint RbMS;
GLuint Rb;
#endif
};                             // namespace FBO
#define TexMS_RGBA TexMS_CMYA  // we can recycle the CMYA for RGBA

enum BlitMode
{
  RESOLVEWITHBLIT = 0,
  RESOLVEWITHSHADERTEX,
  RESOLVEWITHSHADERIMAGE,
  RESOLVERGBATOBACKBUFFER,  // for the RGBA case
};
BlitMode blitMode;

enum MRTMode
{
  RENDER1STEP = 0,
  RENDER2STEPS,
  RENDER1STEPRGBA,
};
MRTMode mrtMode = RENDER1STEP;

GLuint g_vao = 0;

//------------------------------------------------------------------------------
// It is possible that this callback is invoked from another thread
// so let's just append messages for later diplay in the main loop
//------------------------------------------------------------------------------
struct LogMessage
{
  LogMessage(int l, const char* t)
  {
    level = l;
    txt   = t;
  }
  int         level;
  std::string txt;
};
typedef std::list<LogMessage> Messages;
//------------------------------------------------------------------------------
void sample_print(int level, const char* txt) {}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// UI stuff
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

#define COMBO_BLITMODE 0
#define COMBO_MRTMODE 1
#define COMBO_MSAACOL 2
#define COMBO_MSAARAST 3
#define COMBO_BLENDEQ 4
#define COMBO_BLENDFNSRC 5
#define COMBO_BLENDFNDST 6
void MyWindow::processUI(int width, int height, double dt)
{
  // Update imgui configuration
  auto& imgui_io       = ImGui::GetIO();
  imgui_io.DeltaTime   = static_cast<float>(dt);
  imgui_io.DisplaySize = ImVec2(width, height);

  ImGui::NewFrame();
  ImGui::SetNextWindowBgAlpha(0.5);
  ImGui::SetNextWindowSize(ImVec2(450, 0), ImGuiCond_FirstUseEver);
  if(ImGui::Begin("NVIDIA " PROJECT_NAME, nullptr))
  {
    //ImGui::PushItemWidth(200);
    ImGui::Text("gl and vk version");
    ImGuiH::InputIntClamped("N Objs x&y", &g_NObjs, 1, 100);
    ImGui::Separator();
    //
    // Blit modes
    //
    m_guiRegistry.enumCombobox(COMBO_BLITMODE, "Blit Mode", &blitMode);
    //
    // render mode on Muyltiple render-targets
    //
    m_guiRegistry.enumCombobox(COMBO_MRTMODE, "Render Pass", &mrtMode);
    //
    // Color samples combo
    //
    m_guiRegistry.enumCombobox(COMBO_MSAACOL, "MSAA Color samples", &g_CurMSAAColor);
    //
    // Mixed samples combo
    //
    if(g_has_GL_NV_framebuffer_mixed_samples)
    {
      m_guiRegistry.enumCombobox(COMBO_MSAARAST, "MSAA Raster samples", &g_MSAARaster);
    }
    //
    // Blending Equations
    //
    m_guiRegistry.enumCombobox(COMBO_BLENDEQ, "Blend Equation", &g_curBlendEquation);
    //
    // Blending Funcs
    //
    m_guiRegistry.enumCombobox(COMBO_BLENDFNSRC, "Blend Func SRC", &g_blendSRC);
    m_guiRegistry.enumCombobox(COMBO_BLENDFNDST, "Blend Func DST", &g_blendDST);
    //
    // Global transparency
    //
    ImGui::Separator();
    ImGuiH::InputFloatClamped("Global Alpha", &g_alpha, 0.0, 1.0);
    //ImGui::Checkbox("continuous render", &m_realtime.bNonStopRendering);
    ImGui::Checkbox("animation", &s_bCameraAnim);
    ImGui::Separator();
    ImGui::Text("('h' to toggle help)");
    //if(s_bStats)
    //    h += m_oglTextBig.drawString(5, m_winSz[1]-h, hudStats.c_str(), 0, vec4f(0.8,0.8,1.0,0.5).vec_array);

    if(s_helpText)
    {
      ImGui::BeginChild("Help", ImVec2(400, 110), true);
      // camera help
      //ImGui::SetNextWindowCollapsed(0);
      const char* txt = getHelpText();
      ImGui::Text("'`' or 'u' to toggle UI\n");
      ImGui::Text("%s", txt);
      ImGui::EndChild();
    }

    int avg = 100;

    if(s_profiler.getTotalFrames() % avg == avg - 1)
    {
      s_profiler.getAveragedValues("frame", s_statsCpuTime, s_statsGpuTime);
    }

    float gpuTimeF = float(s_statsGpuTime);
    float cpuTimeF = float(s_statsCpuTime);
    float maxTimeF = std::max(std::max(cpuTimeF, gpuTimeF), 0.0001f);

    ImGui::Text("Frame     [ms]: %2.1f", dt * 1000.0f);
    ImGui::Text("Scene GPU [ms]: %2.3f", gpuTimeF / 1000.0f);
    ImGui::Text("Scene CPU [ms]: %2.3f", cpuTimeF / 1000.0f);
    ImGui::ProgressBar(gpuTimeF / maxTimeF, ImVec2(0.0f, 0.0f));
    ImGui::ProgressBar(cpuTimeF / maxTimeF, ImVec2(0.0f, 0.0f));
  }
  ImGui::End();
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// FBO stuff
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

bool CheckFramebufferStatus()
{
  GLenum status;
  status = (GLenum)glCheckFramebufferStatus(GL_FRAMEBUFFER);
  switch(status)
  {
    case GL_FRAMEBUFFER_COMPLETE:
      return true;
    case GL_FRAMEBUFFER_UNSUPPORTED:
      LOGE("Unsupported framebuffer format\n");
      assert(!"Unsupported framebuffer format");
      break;
    case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
      LOGE("Framebuffer incomplete, missing attachment\n");
      assert(!"Framebuffer incomplete, missing attachment");
      break;
    //case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS:
    //	PRINTF(("Framebuffer incomplete, attached images must have same dimensions\n"));
    //	assert(!"Framebuffer incomplete, attached images must have same dimensions");
    //	break;
    //case GL_FRAMEBUFFER_INCOMPLETE_FORMATS:
    //	PRINTF(("Framebuffer incomplete, attached images must have same format\n"));
    //	assert(!"Framebuffer incomplete, attached images must have same format");
    //	break;
    case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
      LOGE("Framebuffer incomplete, missing draw buffer\n");
      assert(!"Framebuffer incomplete, missing draw buffer");
      break;
    case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
      LOGE("Framebuffer incomplete, missing read buffer\n");
      assert(!"Framebuffer incomplete, missing read buffer");
      break;
    case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
      LOGE("Framebuffer incomplete attachment\n");
      assert(!"Framebuffer incomplete attachment");
      break;
    case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
      LOGE("Framebuffer incomplete multisample\n");
      assert(!"Framebuffer incomplete multisample");
      break;
    default:
      LOGE("Error %x\n", status);
      assert(!"unknown FBO Error");
      break;
  }
  return false;
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
GLuint createTexture(int w, int h, int samples, int coverageSamples, GLenum intfmt, GLenum fmt)
{
  GLuint textureID;
  glGenTextures(1, &textureID);
  if(samples <= 1)
  {
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, intfmt, w, h, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  }
  else
  {
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, textureID);
    // Note: fixed-samples set to GL_TRUE, otherwise it could fail when attaching to FBO having render-buffer !!
    if(coverageSamples > 1)
    {
      glTexImage2DMultisampleCoverageNV(GL_TEXTURE_2D_MULTISAMPLE, coverageSamples, samples, intfmt, w, h, GL_TRUE);
    }
    else
    {
      glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples, intfmt, w, h, GL_TRUE);
    }
    // Multi-sample textures don't suupport sampler state settings
    //glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    //glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    //glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    //glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  }
  return textureID;
}
//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
GLuint createTextureRGBA8(int w, int h, int samples, int coverageSamples)
{
  return createTexture(w, h, samples, coverageSamples, GL_RGBA8, GL_RGBA);
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
GLuint createTextureDST(int w, int h, int samples, int coverageSamples)
{
  //return createTexture(w, h, samples, coverageSamples, GL_DEPTH24_STENCIL8, GL_DEPTH24_STENCIL8);
  return createTexture(w, h, samples, coverageSamples, GL_STENCIL_INDEX8, GL_STENCIL_INDEX8);
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
#ifdef USE_RENDERBUFFERS
GLuint createRenderBuffer(int w, int h, int samples, int coverageSamples, GLenum fmt)
{
  int    query;
  GLuint rb;
  glGenRenderbuffers(1, &rb);
  glBindRenderbuffer(GL_RENDERBUFFER, rb);
  if(coverageSamples)
  {
    glRenderbufferStorageMultisampleCoverageNV(GL_RENDERBUFFER, coverageSamples, samples, fmt, w, h);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_COVERAGE_SAMPLES_NV, &query);
    if(query < coverageSamples)
      rb = 0;
    else if(query > coverageSamples)
    {
      // report back the actual number
      coverageSamples = query;
      LOGW("Warning: coverage samples is now %d\n", coverageSamples);
    }
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_COLOR_SAMPLES_NV, &query);
    if(query < samples)
      rb = 0;
    else if(query > samples)
    {
      // report back the actual number
      samples = query;
      LOGW("Warning: depth-samples is now %d\n", samples);
    }
  }
  else
  {
    // create a regular MSAA color buffer
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, fmt, w, h);
    // check the number of samples
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_SAMPLES, &query);

    if(query < samples)
      rb = 0;
    else if(query > samples)
    {
      samples = query;
      LOGW("Warning: depth-samples is now %d\n", samples);
    }
  }
  glBindRenderbuffer(GL_RENDERBUFFER, 0);
  return rb;
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
GLuint createRenderBufferRGBA8(int w, int h, int samples, int coverageSamples)
{
  return createRenderBuffer(w, h, samples, coverageSamples, GL_RGBA8);
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
GLuint createRenderBufferD24S8(int w, int h, int samples, int coverageSamples)
{
  return createRenderBuffer(w, h, samples, coverageSamples, GL_DEPTH24_STENCIL8);
}
//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
GLuint createRenderBufferS8(int w, int h, int samples, int coverageSamples)
{
  return createRenderBuffer(w, h, samples, coverageSamples, GL_STENCIL_INDEX8);
}
#endif
//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
GLuint createFBO()
{
  GLuint fb;
  glGenFramebuffers(1, &fb);
  return fb;
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
bool attachTexture2D(GLuint framebuffer, GLuint textureID, int colorAttachment)
{
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + colorAttachment, GL_TEXTURE_2D, textureID, 0);
  return CheckFramebufferStatus();
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
bool attachTexture2DMS(GLuint framebuffer, GLuint textureID, int colorAttachment)
{
  if(g_MSAAVal[g_CurMSAAColor] <= 1)
    return attachTexture2D(framebuffer, textureID, colorAttachment);
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + colorAttachment, GL_TEXTURE_2D_MULTISAMPLE, textureID, 0);
  return CheckFramebufferStatus();
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
#ifdef USE_RENDERBUFFERS
bool attachRenderbuffer(GLuint framebuffer, GLuint rb, int colorAttachment)
{
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + colorAttachment, GL_RENDERBUFFER, rb);
  return CheckFramebufferStatus();
}
//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
bool attachDSTRenderbuffer(GLuint framebuffer, GLuint dstrb)
{
  bool bRes;
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  //glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, dstrb);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, dstrb);
  return CheckFramebufferStatus();
}
#endif
//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
bool attachDSTTexture2D(GLuint framebuffer, GLuint textureDepthID, GLenum target = GL_TEXTURE_2D)
{
  bool bRes;
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  //glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, target, textureDepthID, 0);
  //bRes = CheckFramebufferStatus();
  //if(!bRes) return false;
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, target, textureDepthID, 0);
  bRes = CheckFramebufferStatus();
  return bRes;
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
bool attachDSTTexture2DMS(GLuint framebuffer, GLuint textureDepthID)
{
  return attachDSTTexture2D(framebuffer, textureDepthID, (g_MSAARaster > 1) ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D);
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
void deleteTexture(GLuint texture)
{
  glDeleteTextures(1, &texture);
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
#ifdef USE_RENDERBUFFERS
void deleteRenderBuffer(GLuint rb)
{
  glDeleteRenderbuffers(1, &rb);
}
#endif
//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
void deleteFBO(GLuint fbo)
{
  glDeleteFramebuffers(1, &fbo);
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
void blitFBO(GLuint srcFBO, GLuint dstFBO, GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLenum filtering)
{
  glBindFramebuffer(GL_READ_FRAMEBUFFER, srcFBO);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dstFBO);
  // GL_NEAREST is needed when Stencil/depth are involved
  glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1,
                    GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT, filtering);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}
//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
void blitFBONearest(GLuint srcFBO, GLuint dstFBO, GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1)
{
  blitFBO(srcFBO, dstFBO, srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, GL_NEAREST);
}
//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
void blitFBOLinear(GLuint srcFBO, GLuint dstFBO, GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1)
{
  blitFBO(srcFBO, dstFBO, srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, GL_LINEAR);
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
void deleteRenderTargets()
{
  if(FBO::TexMS_CMYA_KA_DST)
    deleteFBO(FBO::TexMS_CMYA_KA_DST);
  if(FBO::Tex_CMYA_KA)
    deleteFBO(FBO::Tex_CMYA_KA);
  if(FBO::TexMS_CMYA)
    deleteFBO(FBO::TexMS_CMYA);
  if(FBO::Tex_CMYA)
    deleteFBO(FBO::Tex_CMYA);
  if(FBO::TexMS_KA)
    deleteFBO(FBO::TexMS_KA);
  if(FBO::Tex_KA)
    deleteFBO(FBO::Tex_KA);
#ifdef USE_RENDERBUFFERS
  if(FBO::RbMS)
    deleteFBO(FBO::RbMS);
  if(FBO::Rb)
    deleteFBO(FBO::Rb);
#endif
  if(Texture::CMYA)
    deleteTexture(Texture::CMYA);
  if(Texture::KA)
    deleteTexture(Texture::KA);
  if(Texture::MS_CMYA)
    deleteTexture(Texture::MS_CMYA);
  if(Texture::MS_KA)
    deleteTexture(Texture::MS_KA);
#ifndef USE_RENDERBUFFERS
  if(Texture::MS_DST)
    deleteTexture(Texture::MS_DST);
  if(Texture::DST)
    deleteTexture(Texture::DST);
#else
  if(Renderbuffer::RGBA)
    deleteRenderBuffer(Renderbuffer::RGBA);
  if(Renderbuffer::RGBAMS)
    deleteRenderBuffer(Renderbuffer::RGBAMS);
  if(Renderbuffer::DST)
    deleteRenderBuffer(Renderbuffer::DST);
  if(Renderbuffer::DSTMS)
    deleteRenderBuffer(Renderbuffer::DSTMS);
#endif
  fboSz[0] = 0;
  fboSz[1] = 0;
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
void buildRenderTargets(int w, int h)
{
  deleteRenderTargets();
  if(g_has_GL_NV_framebuffer_mixed_samples)
  {
    if(g_MSAARaster < g_MSAAVal[g_CurMSAAColor])
      g_MSAARaster = g_MSAAVal[g_CurMSAAColor];
  }
  else
    g_MSAARaster = g_MSAAVal[g_CurMSAAColor];

  LOGI("Building Render targets with MSAA Color = %d and MSAA Raster = %d\n", g_MSAAVal[g_CurMSAAColor], g_MSAARaster);
  fboSz[0] = w;
  fboSz[1] = h;
  // a texture
  Texture::CMYA = createTextureRGBA8(w, h, 0, 0);
  Texture::KA   = createTextureRGBA8(w, h, 0, 0);
  // a texture in MSAA
  Texture::MS_CMYA = createTextureRGBA8(w, h, g_MSAAVal[g_CurMSAAColor], 0);
  Texture::MS_KA   = createTextureRGBA8(w, h, g_MSAAVal[g_CurMSAAColor], 0);  // TODO: RA8
#ifndef USE_RENDERBUFFERS
  Texture::DST    = createTextureDST(w, h, 0, 0);
  Texture::MS_DST = createTextureDST(w, h, g_MSAARaster, 0);
#else
  // a renderbuffer
  Renderbuffer::RGBA = createRenderBufferRGBA8(w, h, 0, 0);
  // a renderbuffer in MSAA
  Renderbuffer::RGBAMS = createRenderBufferRGBA8(w, h, g_MSAAVal[g_CurMSAAColor], 0);
  // a depth stencil
  Renderbuffer::DST = createRenderBufferS8 /*D24S8*/ (w, h, 0, 0);
  // a depth stencil in MSAA
  Renderbuffer::DSTMS = createRenderBufferS8 /*D24S8*/ (w, h, g_MSAARaster, 0);
#endif
  // fbo for texture MSAA as the color buffer
  FBO::TexMS_CMYA_KA_DST = createFBO();
  {
#ifdef USE_RENDERBUFFERS
    attachDSTRenderbuffer(FBO::TexMS_CMYA_KA_DST, Renderbuffer::DSTMS);
#else
    attachDSTTexture2DMS(FBO::TexMS_CMYA_KA_DST, Texture::MS_DST);
#endif
    attachTexture2DMS(FBO::TexMS_CMYA_KA_DST, Texture::MS_CMYA, 0);
    attachTexture2DMS(FBO::TexMS_CMYA_KA_DST, Texture::MS_KA, 1);
  }
  // fbo for a texture as the color buffer

  FBO::Tex_CMYA_KA = createFBO();
  {
    attachTexture2D(FBO::Tex_CMYA_KA, Texture::CMYA, 0);
    attachTexture2D(FBO::Tex_CMYA_KA, Texture::KA, 1);
  }
  // fbo for Blit operation
  FBO::Tex_CMYA = createFBO();
  {
    attachTexture2D(FBO::Tex_CMYA, Texture::CMYA, 0);
  }
  FBO::Tex_KA = createFBO();
  {
    attachTexture2D(FBO::Tex_KA, Texture::KA, 0);
  }
  FBO::TexMS_CMYA = createFBO();
  {
    attachTexture2DMS(FBO::TexMS_CMYA, Texture::MS_CMYA, 0);
  }
  FBO::TexMS_KA = createFBO();
  {
    attachTexture2DMS(FBO::TexMS_KA, Texture::MS_KA, 0);
  }
  // fbo for renderbuffer MSAA as the color buffer
#ifdef USE_RENDERBUFFERS
  FBO::RbMS = createFBO();
  {
    attachRenderbuffer(FBO::RbMS, Renderbuffer::RGBAMS, 0);
    //attachDSTRenderbuffer(FBO::RbMS, Renderbuffer::DSTMS);
  }
  // fbo for renderbuffer as the color buffer
  FBO::Rb = createFBO();
  {
    attachRenderbuffer(FBO::Rb, Renderbuffer::RGBA, 0);
    attachDSTRenderbuffer(FBO::Rb, Renderbuffer::DST);
  }
#endif
  // build a VBO for the size of the FBO
  //
  // make a VBO for Quad
  //
  if(g_vboQuad == 0)
    glGenBuffers(1, &g_vboQuad);
  glBindBuffer(GL_ARRAY_BUFFER, g_vboQuad);
  int vertices[2 * 4] = {0, 0, w, 0, 0, h, w, h};
  glBufferData(GL_ARRAY_BUFFER, sizeof(int) * 2 * 4, vertices, GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}
//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
bool MyWindow::open(int posX, int posY, int width, int height, const char* title, const nvgl::ContextWindowCreateInfo& context)
{
  if(!AppWindowCameraInertia::open(posX, posY, width, height, title, true))
    return false;
  m_contextWindowGL.init(&context, m_internal, title);
  ImGui::InitGL();
  s_profiler.init();


  m_camera.curEyePos = m_camera.eyePos = vec3f(0, 0, 3.0f);
  LOGI("'1': Blit used for MSAA resolve");
  LOGI("'2': TexelFetch used on MSAA Texture to resolve");
  LOGI("'3': ImageLoad used on MSAA Texture to resolve");
  //
  // UI
  //
  auto& imgui_io       = ImGui::GetIO();
  imgui_io.IniFilename = nullptr;
  m_guiRegistry.enumAdd(COMBO_BLITMODE, RESOLVEWITHBLIT, "Resolve with Blit");
  m_guiRegistry.enumAdd(COMBO_BLITMODE, RESOLVEWITHSHADERTEX, "Resolve with Shader&Texture Fetch");
  m_guiRegistry.enumAdd(COMBO_BLITMODE, RESOLVEWITHSHADERIMAGE, "Resolve with Shader&Image Load");
  m_guiRegistry.enumAdd(COMBO_BLITMODE, RESOLVERGBATOBACKBUFFER, "Resolve with blit : RGBA to backbuffer");
  m_guiRegistry.enumAdd(COMBO_MRTMODE, RENDER1STEP, "Render CMYA & KA to 2 RTs at a time");
  m_guiRegistry.enumAdd(COMBO_MRTMODE, RENDER2STEPS, "Render CMYA to RT#0 then KA to RT#1");
  m_guiRegistry.enumAdd(COMBO_MRTMODE, RENDER1STEPRGBA, "Render RGBA to RT#0 only");
  m_guiRegistry.enumAdd(COMBO_MSAACOL, 0, "MSAA OFF");
  m_guiRegistry.enumAdd(COMBO_MSAACOL, 1, "MSAA 2x");
  m_guiRegistry.enumAdd(COMBO_MSAACOL, 2, "MSAA 8x");

  // TODO check "GL_NV_framebuffer_mixed_samples")
  {
    g_has_GL_NV_framebuffer_mixed_samples = true;
    m_guiRegistry.enumAdd(COMBO_MSAARAST, 2, "MSAA 2x");
    m_guiRegistry.enumAdd(COMBO_MSAARAST, 4, "MSAA 4x");
    m_guiRegistry.enumAdd(COMBO_MSAARAST, 8, "MSAA 8x");
    m_guiRegistry.enumAdd(COMBO_MSAARAST, 16, "MSAA 16x");
  }
  for(int be = 0; blendequations[be] != GL_ZERO; be++)
  {
    m_guiRegistry.enumAdd(COMBO_BLENDEQ, be, blendequationNames[be]);
  }
  for(int be = 0; blendfuncs[be] != GL_ZERO; be++)
  {
    m_guiRegistry.enumAdd(COMBO_BLENDFNSRC, be, blendfuncNames[be]);
  }
  for(int be = 0; blendfuncs[be] != GL_ZERO; be++)
  {
    m_guiRegistry.enumAdd(COMBO_BLENDFNDST, be, blendfuncNames[be]);
  }

  m_realtime.bNonStopRendering = true;
  //
  // Shader Programs for rasterization
  //
  if(!g_prog_Cst_OneMinusCMYK_A.compileProgram(g_glslv_WVP_Position, NULL, g_glslf_OneMinusCMYK_A))
    return false;
  if(!g_progPR_Cst_OneMinusCMYK_A.compileProgram(NULL, NULL, g_glslf_OneMinusCMYK_A))
    return false;
  if(!g_prog_Cst_RGBA.compileProgram(g_glslv_WVP_Position, NULL, g_glslf_RGBA))
    return false;
  if(!g_progPR_Cst_RGBA.compileProgram(NULL, NULL, g_glslf_RGBA))
    return false;
  //
  // Shader Programs for fullscreen processing
  //
  for(int i = 0; i < 3; i++)
  {
    if(!g_progTexMS_CMYA_KA_2_RGBA[i].compileProgram(g_glslv_Tc, NULL, g_glslf_TexMS_CMYA_KA_2_RGBA[i]))
      return false;
    if(!g_progImageMS_CMYA_KA_2_RGBA[i].compileProgram(g_glslv_Tc, NULL, g_glslf_ImageMS_CMYA_KA_2_RGBA[i]))
      return false;
  }
  if(!g_progTex_CMYA_KA_2_RGBA.compileProgram(g_glslv_Tc, NULL, g_glslf_Tex_CMYA_KA_2_RGBA))
    return false;
  //
  // Misc OGL setup
  //
  glClearColor(0.0f, 0.1f, 0.1f, 1.0f);
  glGenVertexArrays(1, &g_vao);
  glBindVertexArray(g_vao);
  //
  // Circle
  //
  glGenBuffers(1, &g_vboCircle);
  glBindBuffer(GL_ARRAY_BUFFER, g_vboCircle);
#define SUBDIVS 180
#define CIRCLESZ 0.5f
  vec3f* data = new vec3f[SUBDIVS + 2];
  vec3f* p    = data;
  int    j    = 0;
  *(p++)      = vec3f(0, 0, 0);
  for(int i = 0; i < SUBDIVS + 1; i++)
  {
    float a = nv_to_rad * (float)i * (360.0f / (float)SUBDIVS);
    vec3f v(CIRCLESZ * cosf(a), CIRCLESZ * sinf(a), 0.0f);
    *(p++) = v;
  }
  glBufferData(GL_ARRAY_BUFFER, sizeof(vec3f) * (SUBDIVS + 2), data[0].vec_array, GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  //
  // NV-Path rendering
  //
  // TODO check GL_NV_path_rendering
  {
    g_usePathObj                          = true;
    g_pathObj                             = glGenPathsNV(1);
    static const GLubyte pathCommands[10] = {
        GL_MOVE_TO_NV,    GL_LINE_TO_NV, GL_LINE_TO_NV, GL_LINE_TO_NV, GL_LINE_TO_NV,
        GL_CLOSE_PATH_NV, 'M',           'C',           'C',           'Z'};  // character aliases
    static const float scale = 1.0f / 500.0f;
    static const float x0    = 250.0f;
    static const float y0    = 250.0f;
#define COORD(x, y)                                                                                                    \
  {                                                                                                                    \
    scale*(x - x0), scale*(y - y0)                                                                                     \
  }
    static const GLfloat pathCoords[12][2] = {COORD(100, 180), COORD(40, 10),   COORD(190, 120), COORD(10, 120),
                                              COORD(160, 10),  COORD(300, 300), COORD(100, 400), COORD(100, 200),
                                              COORD(300, 100), COORD(500, 200), COORD(500, 400), COORD(300, 300)};
    glPathCommandsNV(g_pathObj, 10, pathCommands, 24, GL_FLOAT, pathCoords);

    glPathParameterfNV(g_pathObj, GL_PATH_STROKE_WIDTH_NV, 0.01f);
    glPathParameteriNV(g_pathObj, GL_PATH_JOIN_STYLE_NV, GL_ROUND_NV);
  }
  //else {
  //    g_usePathObj = false;
  //}
  //
  // NV_framebuffer_mixed_samples
  //
  // TODO check GL_NV_framebuffer_mixed_samples
  {
    g_MSAARaster = 8;
    //glEnable(GL_RASTER_MULTISAMPLE);
    //glRasterSamplesEXT(g_MSAARaster, GL_TRUE);
    glCoverageModulationNV(GL_RGBA);
    LOGI("GL_NV_framebuffer_mixed_samples detected: color MSAA= %d; MSAA for DST=%d\n", g_MSAAVal[g_CurMSAAColor], g_MSAARaster);
  }
  // --------------------------------------------
  // FBOs
  //
  buildRenderTargets(getWidth(), getHeight());

  m_validated = true;
  return true;
}
//------------------------------------------------------------------------------
void MyWindow::onWindowClose()
{
  s_profiler.deinit();
  ImGui::ShutdownGL();
  AppWindowCameraInertia::onWindowClose();
  m_contextWindowGL.deinit();
}

//------------------------------------------------------------------------------
void MyWindow::onWindowResize(int w, int h)
{
  AppWindowCameraInertia::onWindowResize(w, h);
  glMatrixMode(GL_PROJECTION);
  glLoadMatrixf(m_projection.mat_array);
  glMatrixMode(GL_MODELVIEW);
  //
  // rebuild the FBOs to match the new size
  //
  if(m_validated)
    buildRenderTargets(w, h);
}

//------------------------------------------------------------------------------
#define KEYTAU 0.10f
void MyWindow::onKeyboard(NVPWindow::KeyCode key, MyWindow::ButtonAction action, int mods, int x, int y)
{
  AppWindowCameraInertia::onKeyboard(key, action, mods, x, y);
  if(action == MyWindow::BUTTON_RELEASE)
    return;

  switch(key)
  {
    case NVPWindow::KEY_F1:
      s_profiler.reset(1);
      break;
      //...
    case NVPWindow::KEY_F12:
      break;
  }
}
//------------------------------------------------------------------------------
void MyWindow::onKeyboardChar(unsigned char key, int mods, int x, int y)
{
  AppWindowCameraInertia::onKeyboardChar(key, mods, x, y);
  switch(key)
  {
    case '1':
      blitMode = RESOLVEWITHBLIT;
      LOGI("blitting using framebufferblit()\n");
      break;
    case '2':
      blitMode = RESOLVEWITHSHADERTEX;
      LOGI("blitting using fullscreenquad and texture\n");
      break;
    case '3':
      blitMode = RESOLVEWITHSHADERIMAGE;
      LOGI("blitting using fullscreenquad and image\n");
      break;
    case '4':
      blitMode = RESOLVERGBATOBACKBUFFER;
      LOGI("blitting using fullscreenquad and image to backbuffer\n");
      break;
    case '0':
      m_camera.print_look_at();
      break;
    case 'h':
      s_helpText ^= 1;
      break;
    case ' ':
      s_bCameraAnim ^= 1;
      break;
    case '`':
    case 'u':
      g_buseUI ^= 1;
      break;
    default:
      break;
  }
}

//////////////////////////////////////////////////////////////////////////////
// Circles
//
void beginCircles()
{
  glDisable(GL_STENCIL_TEST);
  if(mrtMode == RENDER1STEPRGBA)  // in RGBA, only provide the simple rendering to one render-target, using RGBA
    g_prog_Cst_RGBA.enable();
  else
    g_prog_Cst_OneMinusCMYK_A.enable();  // using g_glslf_OneMinusCMYK_A
  glBindBuffer(GL_ARRAY_BUFFER, g_vboCircle);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vec3f), NULL);
}
void drawFilledCircle(mat4f mWVP, vec3f& p, float scale, vec4f& CMYK, float alpha)
{
  mWVP.translate(p);
  mWVP.scale(scale);
  g_prog_Cst_OneMinusCMYK_A.setUniformMatrix4fv("mWVP", mWVP.mat_array, false);
  g_prog_Cst_OneMinusCMYK_A.setUniform1f("alpha", alpha);
  if(mrtMode == RENDER1STEPRGBA)  // in RGBA, only provide the simple rendering to one render-target, using RGBA
  {
    vec3f RGB = convertCMYK2RGB(CMYK);
    g_prog_Cst_RGBA.setUniform4f("RGBA", RGB[0], RGB[1], RGB[2], alpha);
    glDrawArrays(GL_TRIANGLE_FAN, 0, SUBDIVS + 2);
    g_prog_Cst_RGBA.setUniform4f("RGBA", 0.0f, 0.0f, 0.0f, alpha);
    glDrawArrays(GL_LINE_STRIP, 1, SUBDIVS + 1);
  }
  else if(mrtMode == RENDER1STEP)
  {
    g_prog_Cst_OneMinusCMYK_A.setUniform4f("CMYK", CMYK[0], CMYK[1], CMYK[2], CMYK[3]);
    glDrawArrays(GL_TRIANGLE_FAN, 0, SUBDIVS + 2);
    g_prog_Cst_OneMinusCMYK_A.setUniform4f("CMYK", 0.0f, 0.0f, 0.0f, 1.0f);
    glDrawArrays(GL_LINE_STRIP, 1, SUBDIVS + 1);
  }
  else
  {
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    g_prog_Cst_OneMinusCMYK_A.setUniform4f("CMYK", CMYK[0], CMYK[1], CMYK[2]);
    glDrawArrays(GL_TRIANGLE_FAN, 0, SUBDIVS + 2);
    if(g_curBlendEquation > 0)
      glBlendBarrierNV();
    g_prog_Cst_OneMinusCMYK_A.setUniform4f("CMYK", 0.0f, 0.0f, 0.0f);
    glDrawArrays(GL_LINE_STRIP, 1, SUBDIVS + 1);
    if(g_curBlendEquation > 0)
      glBlendBarrierNV();
    glDrawBuffer(GL_COLOR_ATTACHMENT1);
    g_prog_Cst_OneMinusCMYK_A.setUniform4f("CMYK", CMYK[3]);
    glDrawArrays(GL_TRIANGLE_FAN, 0, SUBDIVS + 2);
    if(g_curBlendEquation > 0)
      glBlendBarrierNV();
    g_prog_Cst_OneMinusCMYK_A.setUniform4f("CMYK", 1.0f);
    glDrawArrays(GL_LINE_STRIP, 1, SUBDIVS + 1);
    if(g_curBlendEquation > 0)
      glBlendBarrierNV();
  }
}
void endCircles()
{
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glDisableVertexAttribArray(0);
  if(mrtMode == RENDER1STEPRGBA)  // in RGBA, only provide the simple rendering to one render-target, using RGBA
    g_prog_Cst_RGBA.enable();
  else
    g_prog_Cst_OneMinusCMYK_A.disable();
}
//////////////////////////////////////////////////////////////////////////////
// simple path object
// http://developer.download.nvidia.com/assets/gamedev/files/GL_NV_path_rendering.txt
//
void beginPath(mat4f& mWV)
{
  glEnable(GL_STENCIL_TEST);
  glLoadMatrixf(mWV.mat_array);
  if(mrtMode == RENDER1STEPRGBA)  // in RGBA, only provide the simple rendering to one render-target, using RGBA
    g_progPR_Cst_RGBA.enable();
  else
    g_progPR_Cst_OneMinusCMYK_A.enable();
}
void drawPath(vec3f& p, float scale, vec4f& CMYK, float alpha)
{
  glDisableVertexAttribArray(0);
  glPushMatrix();
  glTranslatef(p.x, p.y, p.z);
  glScalef(scale, scale, scale);
  glPathStencilFuncNV(GL_ALWAYS, 0, 0xFF);

  glStencilFillPathNV(g_pathObj, GL_INVERT /*GL_COUNT_UP_NV*/, 0xFF);
  glStencilFunc(GL_NOTEQUAL, /*stencil_ref*/ 0, /*read_mask*/ 0xFF);
  if(mrtMode == RENDER1STEPRGBA)  // in RGBA, only provide the simple rendering to one render-target, using RGBA
  {
    vec3f RGB = convertCMYK2RGB(CMYK);
    g_progPR_Cst_RGBA.setUniform4f("RGBA", RGB[0], RGB[1], RGB[2], alpha);
    glStencilOp(GL_ZERO, GL_ZERO, GL_ZERO);  // sfail: s failed, dpfail: s passed/d failed, dppass: s and d passed
    glCoverFillPathNV(g_pathObj, GL_CONVEX_HULL_NV);
  }
  else if(mrtMode == RENDER1STEP)
  {
    g_progPR_Cst_OneMinusCMYK_A.setUniform1f("alpha", alpha);
    g_progPR_Cst_OneMinusCMYK_A.setUniform4f("CMYK", CMYK[0], CMYK[1], CMYK[2], CMYK[3]);
    glStencilOp(GL_ZERO, GL_ZERO, GL_ZERO);  // sfail: s failed, dpfail: s passed/d failed, dppass: s and d passed
    glCoverFillPathNV(g_pathObj, GL_CONVEX_HULL_NV);
  }
  else  //if(mrtMode == RENDER2STEPS)
  {
    g_progPR_Cst_OneMinusCMYK_A.setUniform1f("alpha", alpha);
    g_progPR_Cst_OneMinusCMYK_A.setUniform4f("CMYK", CMYK[0], CMYK[1], CMYK[2]);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);  // sfail: s failed, dpfail: s passed/d failed, dppass: s and d passed
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glCoverFillPathNV(g_pathObj, GL_CONVEX_HULL_NV);
    g_progPR_Cst_OneMinusCMYK_A.setUniform4f("CMYK", CMYK[3]);
    glStencilOp(GL_ZERO, GL_ZERO, GL_ZERO);  // sfail: s failed, dpfail: s passed/d failed, dppass: s and d passed
    glDrawBuffer(GL_COLOR_ATTACHMENT1);
    glCoverFillPathNV(g_pathObj, GL_CONVEX_HULL_NV);
  }

  glStencilStrokePathNV(g_pathObj, GL_INVERT /*GL_COUNT_UP_NV*/, 0xFF);
  if(mrtMode == RENDER1STEPRGBA)  // in RGBA, only provide the simple rendering to one render-target, using RGBA
  {
    g_progPR_Cst_RGBA.setUniform4f("RGBA", 0.0f, 0.0f, 0.0f, alpha);
    glStencilOp(GL_ZERO, GL_ZERO, GL_ZERO);  // sfail: s failed, dpfail: s passed/d failed, dppass: s and d passed
    glCoverFillPathNV(g_pathObj, GL_CONVEX_HULL_NV);
  }
  else if(mrtMode == RENDER1STEP)
  {
    g_prog_Cst_OneMinusCMYK_A.setUniform4f("CMYK", 0.0f, 0.0f, 0.0f, 1.0f);
    glCoverStrokePathNV(g_pathObj, GL_CONVEX_HULL_NV);
  }
  else  //if(mrtMode == RENDER2STEPS)
  {
    g_prog_Cst_OneMinusCMYK_A.setUniform4f("CMYK", 0.0f, 0.0f, 0.0f);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);  // sfail: s failed, dpfail: s passed/d failed, dppass: s and d passed
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glCoverStrokePathNV(g_pathObj, GL_CONVEX_HULL_NV);
    g_prog_Cst_OneMinusCMYK_A.setUniform4f("CMYK", /*K*/ 1.0f);
    glStencilOp(GL_ZERO, GL_ZERO, GL_ZERO);  // sfail: s failed, dpfail: s passed/d failed, dppass: s and d passed
    glDrawBuffer(GL_COLOR_ATTACHMENT1);
    glCoverStrokePathNV(g_pathObj, GL_CONVEX_HULL_NV);
  }
  glPopMatrix();
}
void endPath()
{
  glDisable(GL_STENCIL_TEST);
  if(mrtMode == RENDER1STEPRGBA)  // in RGBA, only provide the simple rendering to one render-target, using RGBA
    g_progPR_Cst_RGBA.disable();
  else
    g_progPR_Cst_OneMinusCMYK_A.disable();
}
//////////////////////////////////////////////////////////////////////////////
// scene
//

//------------------------------------------------------------------------------
void MyWindow::renderScene()
{
  PROFILE_SECTION(__FUNCTION__);
  static vec4f colorsCMYK[] = {vec4f(1, 1, 0, 0.0), vec4f(0, 1, 1, 0.0), vec4f(1, 0, 1, 0.0),    vec4f(1, 0, 0, 0.0),
                               vec4f(0, 1, 0, 0.0), vec4f(0, 0, 1, 0.0), vec4f(0.4f, 0, 1, 0.4f)};
  if(g_pathObj && g_usePathObj)
  {
    // m_projection is already in glMatrixMode(GL_PROJECTION)
    beginPath(m_camera.m4_view);
    {
      int c = 0;
      for(int y = 0; y < g_NObjs; y++)
        for(int x = 0; x < g_NObjs; x++)
        {
          vec3f p(-1.0f + 2.0f * (float)x / (float)g_NObjs, -1.0f + 2.0f * (float)y / (float)g_NObjs, 0);
          drawPath(p, 1.0f, colorsCMYK[c], g_alpha);
          c = (c + 1) % 7;
        }
    }
    endPath();
  }
  else
  {
    mat4f mVP;
    mVP = m_projection * m_camera.m4_view;
    beginCircles();
    {
      int c = 0;
      for(int y = 0; y < g_NObjs; y++)
        for(int x = 0; x < g_NObjs; x++)
        {
          vec3f p(-1.0f + 2.0f * (float)x / (float)g_NObjs, -1.0f + 2.0f * (float)y / (float)g_NObjs, 0);
          drawFilledCircle(mVP, p, 1.0f, colorsCMYK[c], g_alpha);
          c = (c + 1) % 7;
        }
    }
    endCircles();
  }
}

//------------------------------------------------------------------------------
void renderFullscreenQuad()
{
  glBindBuffer(GL_ARRAY_BUFFER, g_vboQuad);
  glEnableVertexAttribArray(0);
  glVertexAttribIPointer(0, 2, GL_INT, sizeof(int) * 2, NULL);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glDisableVertexAttribArray(0);
}

//------------------------------------------------------------------------------
void MyWindow::onWindowRefresh()
{
  AppWindowCameraInertia::onWindowRefresh();
  float dt = (float)m_realtime.getFrameDT();
  //
  // Simple camera change for animation
  //
  if(s_bCameraAnim)
  {
    s_cameraAnimIntervals -= dt;
    if(s_cameraAnimIntervals <= 0.0)
    {
      s_cameraAnimIntervals = ANIMINTERVALL;
      m_camera.look_at(s_cameraAnim[s_cameraAnimItem].eye, s_cameraAnim[s_cameraAnimItem].focus);
      s_cameraAnimItem++;
      if(s_cameraAnimItem >= s_cameraAnimItems)
        s_cameraAnimItem = 0;
    }
  }
  s_profiler.beginFrame();
  {
    PROFILE_SECTION("frame");
    NXPROFILEFUNC("frame");
    //glEnable(GL_FRAMEBUFFER_SRGB);
    //
    // Render with some CMYK colored primitives into a 2-render target destination
    // the result will be stored in 1-(value)
    //
    const vec4f bgngCMYK(0.0f, 0.0f, 0.0f, 0.0f);
    glDisable(GL_DEPTH_TEST);

    // Blending
    if(g_blendEnable)
      glEnable(GL_BLEND);
    else
      glDisable(GL_BLEND);
    glBlendEquation(blendequations[g_curBlendEquation]);
    if(glBlendParameteriNV != NULL)
      glBlendParameteriNV(GL_BLEND_PREMULTIPLIED_SRC_NV, GL_FALSE);
    glBlendFunc(blendfuncs[g_blendSRC], blendfuncs[g_blendDST]);

    // Bind the render targets
    glBindFramebuffer(GL_FRAMEBUFFER, FBO::TexMS_CMYA_KA_DST);

    // Clear for RT#0
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glClearColor(1.0f - bgngCMYK.x, 1.0f - bgngCMYK.y, 1.0f - bgngCMYK.z, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    // Clear for RT#1
    glDrawBuffer(GL_COLOR_ATTACHMENT1);
    glClearColor(1.0f - bgngCMYK.w, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    switch(mrtMode)
    {
      case RENDER1STEP:
        // we can do early Activation of RT#0 & RT#1
        glDrawBuffers(2, drawBuffers);
        renderScene();
        break;
      case RENDER2STEPS:
        // Drawbuffer target must be setup later
        renderScene();
        break;
      case RENDER1STEPRGBA:
        // we can do early Activation of RT#0
        glDrawBuffer(GL_COLOR_ATTACHMENT0);
        renderScene();
        break;
    }
    // back to regular default
    glBlendEquation(GL_FUNC_ADD);
    glDisable(GL_BLEND);
    // Now we have rendered things in 2 render targets
    // 2 solutions:
    //  1- resolve to 2 intermediate renter Textures
    //  2- use a Shader to directly read the 2 MSAA Textures and resolve on the flight
    // Done. Back to the backbuffer
    switch(blitMode)
    {
      case RESOLVEWITHBLIT:
        glDisable(GL_FRAMEBUFFER_SRGB);
        // use the HW Blit to resolve MSAA to regular texture
        glDrawBuffers(1, drawBuffers);
        // Render Target 1
        blitFBONearest(FBO::TexMS_CMYA, FBO::Tex_CMYA, 0, 0, getWidth(), getHeight(), 0, 0, getWidth(), getHeight());
        // Render Target 2
        blitFBONearest(FBO::TexMS_KA, FBO::Tex_KA, 0, 0, getWidth(), getHeight(), 0, 0, getWidth(), getHeight());
        // switch back to our backbuffer and perform the conversion to RGBA
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        g_progTex_CMYA_KA_2_RGBA.enable();  // using g_glslf_Tex_CMYA_KA_2_RGBA
        g_progTex_CMYA_KA_2_RGBA.setUniform2i("viewportSz", getWidth(), getHeight());
        g_progTex_CMYA_KA_2_RGBA.setUniform4f("CMYK_Mask", g_activeC ? 1.0f : 0.0f, g_activeM ? 1.0f : 0.0f,
                                              g_activeY ? 1.0f : 0.0f, g_activeK ? 1.0f : 0.0f);
        g_progTex_CMYA_KA_2_RGBA.bindTexture("sampler_CMYA", Texture::CMYA, GL_TEXTURE_2D, 0);
        g_progTex_CMYA_KA_2_RGBA.bindTexture("sampler_KA", Texture::KA, GL_TEXTURE_2D, 1);
        // Fullscreen quad
        renderFullscreenQuad();
        break;
      case RESOLVEWITHSHADERTEX:
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        g_progTexMS_CMYA_KA_2_RGBA[g_CurMSAAColor].enable();  // using g_glslf_TexMS_CMYA_KA_2_RGBA
        g_progTexMS_CMYA_KA_2_RGBA[g_CurMSAAColor].setUniform2i("viewportSz", getWidth(), getHeight());
        g_progTexMS_CMYA_KA_2_RGBA[g_CurMSAAColor].setUniform4f("CMYK_Mask", g_activeC ? 1.0f : 0.0f, g_activeM ? 1.0f : 0.0f,
                                                                g_activeY ? 1.0f : 0.0f, g_activeK ? 1.0f : 0.0f);
        g_progTexMS_CMYA_KA_2_RGBA[g_CurMSAAColor].bindTexture("samplerMS_CMYA", Texture::MS_CMYA, GL_TEXTURE_2D_MULTISAMPLE, 0);
        g_progTexMS_CMYA_KA_2_RGBA[g_CurMSAAColor].bindTexture("samplerMS_KA", Texture::MS_KA, GL_TEXTURE_2D_MULTISAMPLE, 1);
        // Fullscreen quad
        renderFullscreenQuad();
        break;
      case RESOLVEWITHSHADERIMAGE:
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        if(g_progImageMS_CMYA_KA_2_RGBA[g_CurMSAAColor].getProgId())
        {
          g_progImageMS_CMYA_KA_2_RGBA[g_CurMSAAColor].enable();
          g_progImageMS_CMYA_KA_2_RGBA[g_CurMSAAColor].setUniform2i("viewportSz", getWidth(), getHeight());
          g_progImageMS_CMYA_KA_2_RGBA[g_CurMSAAColor].setUniform4f("CMYK_Mask", g_activeC ? 1.0f : 0.0f, g_activeM ? 1.0f : 0.0f,
                                                                    g_activeY ? 1.0f : 0.0f, g_activeK ? 1.0f : 0.0f);
          g_progImageMS_CMYA_KA_2_RGBA[g_CurMSAAColor].bindImage("imageMS_CMYA", 0, Texture::MS_CMYA, 0, GL_FALSE, 0,
                                                                 GL_READ_ONLY, GL_RGBA8);
          g_progImageMS_CMYA_KA_2_RGBA[g_CurMSAAColor].bindImage("imageMS_KA", 1, Texture::MS_KA, 0, GL_FALSE, 0,
                                                                 GL_READ_ONLY, GL_RGBA8);
          // Fullscreen quad
          renderFullscreenQuad();
        }
        else
        {
          glClearColor(1.0, 0.0, 0.0, 1.0f);
          glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        }
        break;
      case RESOLVERGBATOBACKBUFFER:
        blitFBONearest(FBO::TexMS_RGBA, 0 /*backbuffer*/, 0, 0, getWidth(), getHeight(), 0, 0, getWidth(), getHeight());
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        break;
    }
    if(g_buseUI)
    {
      int width  = getWidth();
      int height = getHeight();
      processUI(width, height, dt);
      ImDrawData* imguiDrawData;
      ImGui::Render();
      imguiDrawData = ImGui::GetDrawData();
      ImGui::RenderDrawDataGL(imguiDrawData);
      ImGui::EndFrame();
    }
  }  //PROFILE_SECTION("frame");
  m_contextWindowGL.swapBuffers();
  s_profiler.endFrame();
}
/////////////////////////////////////////////////////////////////////////
// Main initialization point
//
int main(int argc, const char** argv)
{
  NVPSystem system(PROJECT_NAME);

  // you can create more than only one
  static MyWindow myWindow;

  nvgl::ContextWindowCreateInfo context(4,      //major;
                                        3,      //minor;
                                        false,  //core;
                                        8,      //MSAA;
                                        24,     //depth bits
                                        8,      //stencil bits
                                        true,   //debug;
                                        false,  //robust;
                                        false,  //forward;
                                        false,  //stereo
                                        NULL    //share;
  );

  if(!myWindow.open(0, 0, 1280, 720, "CMYK", context))
  {
    LOGE("Failed to initialize the sample\n");
    return EXIT_FAILURE;
  }

  myWindow.m_contextWindowGL.makeContextCurrent();
  myWindow.m_contextWindowGL.swapInterval(0);
  myWindow.onWindowResize(myWindow.getWidth(), myWindow.getHeight());

  while(myWindow.pollEvents())
  {
    myWindow.idle();
    if(myWindow.m_renderCnt > 0)
    {
      myWindow.m_renderCnt--;
      myWindow.onWindowRefresh();
    }

    if(myWindow.m_guiRegistry.checkValueChange(COMBO_MSAACOL))
    {
      if(!g_has_GL_NV_framebuffer_mixed_samples)
      {
        g_MSAARaster = g_MSAAVal[g_CurMSAAColor];
      }
      buildRenderTargets(myWindow.getWidth(), myWindow.getHeight());
    }
    if(myWindow.m_guiRegistry.checkValueChange(COMBO_MSAARAST))
    {
      buildRenderTargets(myWindow.getWidth(), myWindow.getHeight());
    }
    if(myWindow.m_guiRegistry.checkValueChange(COMBO_BLENDEQ))
    {
      if(g_curBlendEquation == GL_ZERO)
      {  // CHECK THIS!!
        if(mrtMode != RENDER1STEPRGBA)
          mrtMode = RENDER2STEPS;  // need to render only in one render-target at a time
      }
    }
  }
  return EXIT_SUCCESS;
}

#include "Internal3D.h"
#include "Visual3D.h"
#include "Seis3D_Sim.h"
#include "Cuda_Seis3D.cuh"
#include "ResourcePaths.h"
#include "imgui.h"
#include "implot.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace Seis3D::Detail3D {
// Preserve OpenGL bindings and pixel-transfer state around the independent renderer.
struct GLStateScope3D {
    GLint program3D{},vao3D{},drawFramebuffer3D{},readFramebuffer3D{},viewport3D[4]{};
    GLint activeTexture3D{},texture2D{},texture3D{},sampler3D{},buffer3D{};
    GLint unpackAlignment3D{},unpackRowLength3D{},unpackImageHeight3D{},skipPixels3D{},skipRows3D{},skipImages3D{};
    GLboolean blend3D{},depth3D{},scissor3D{},cull3D{};
    // Snapshot all modified graphics state and select tightly packed, unsampled texture uploads.
    GLStateScope3D() {
        glGetIntegerv(GL_CURRENT_PROGRAM,&program3D); glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao3D);
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&drawFramebuffer3D);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&readFramebuffer3D); glGetIntegerv(GL_VIEWPORT,viewport3D);
        glGetIntegerv(GL_ACTIVE_TEXTURE,&activeTexture3D); glActiveTexture(GL_TEXTURE0);
        glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture2D); glGetIntegerv(GL_TEXTURE_BINDING_3D,&texture3D);
        glGetIntegeri_v(GL_SAMPLER_BINDING,0,&sampler3D); glBindSampler(0,0);
        glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING,&buffer3D);
        glGetIntegerv(GL_UNPACK_ALIGNMENT,&unpackAlignment3D); glGetIntegerv(GL_UNPACK_ROW_LENGTH,&unpackRowLength3D);
        glGetIntegerv(GL_UNPACK_IMAGE_HEIGHT,&unpackImageHeight3D); glGetIntegerv(GL_UNPACK_SKIP_PIXELS,&skipPixels3D);
        glGetIntegerv(GL_UNPACK_SKIP_ROWS,&skipRows3D); glGetIntegerv(GL_UNPACK_SKIP_IMAGES,&skipImages3D);
        blend3D=glIsEnabled(GL_BLEND); depth3D=glIsEnabled(GL_DEPTH_TEST);
        scissor3D=glIsEnabled(GL_SCISSOR_TEST); cull3D=glIsEnabled(GL_CULL_FACE);
        glDisable(GL_BLEND); glDisable(GL_DEPTH_TEST); glDisable(GL_SCISSOR_TEST); glDisable(GL_CULL_FACE);
        glPixelStorei(GL_UNPACK_ALIGNMENT,1); glPixelStorei(GL_UNPACK_ROW_LENGTH,0);
        glPixelStorei(GL_UNPACK_IMAGE_HEIGHT,0); glPixelStorei(GL_UNPACK_SKIP_PIXELS,0);
        glPixelStorei(GL_UNPACK_SKIP_ROWS,0); glPixelStorei(GL_UNPACK_SKIP_IMAGES,0);
    }
    // Restore the host application's GL state even if initialization or an upload throws.
    ~GLStateScope3D() {
        glUseProgram(program3D); glBindVertexArray(vao3D);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER,drawFramebuffer3D); glBindFramebuffer(GL_READ_FRAMEBUFFER,readFramebuffer3D);
        glViewport(viewport3D[0],viewport3D[1],viewport3D[2],viewport3D[3]);
        glBindTexture(GL_TEXTURE_2D,texture2D); glBindTexture(GL_TEXTURE_3D,texture3D); glBindSampler(0,sampler3D);
        glActiveTexture(activeTexture3D); glBindBuffer(GL_PIXEL_UNPACK_BUFFER,buffer3D);
        glPixelStorei(GL_UNPACK_ALIGNMENT,unpackAlignment3D); glPixelStorei(GL_UNPACK_ROW_LENGTH,unpackRowLength3D);
        glPixelStorei(GL_UNPACK_IMAGE_HEIGHT,unpackImageHeight3D); glPixelStorei(GL_UNPACK_SKIP_PIXELS,skipPixels3D);
        glPixelStorei(GL_UNPACK_SKIP_ROWS,skipRows3D); glPixelStorei(GL_UNPACK_SKIP_IMAGES,skipImages3D);
        if (blend3D) glEnable(GL_BLEND); if (depth3D) glEnable(GL_DEPTH_TEST);
        if (scissor3D) glEnable(GL_SCISSOR_TEST); if (cull3D) glEnable(GL_CULL_FACE);
    }
};

// Read only deployed 3-D shader resources; this does not depend on 2-D shader state or cwd.
std::string readShader3D(const char* filename) {
    const auto path=getExecutableDirectory()/"resource_Seis3D"/filename;
    std::ifstream input(path,std::ios::binary);
    if (!input) throw std::runtime_error("Seis3D shader unavailable: "+pathToUtf8(path));
    std::ostringstream source; source<<input.rdbuf();
    if (!input.good() && !input.eof()) throw std::runtime_error("Cannot read Seis3D shader.");
    auto result=source.str();
    const std::string marker="#include \"seis3d_transfer.glsl\"";
    if(const auto offset=result.find(marker); offset!=std::string::npos)
        result.replace(offset,marker.size(),readShader3D("seis3d_transfer.glsl"));
    return result;
}

// Compile one shader with its full error log and immediate failure cleanup.
GLuint compileShader3D(GLenum type, const std::string& source) {
    const GLuint shader=glCreateShader(type);
    if (!shader) throw std::runtime_error("Cannot create Seis3D shader.");
    const char* text=source.c_str(); glShaderSource(shader,1,&text,nullptr); glCompileShader(shader);
    GLint ok=0; glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
    if (!ok) {
        char log[4096]{}; glGetShaderInfoLog(shader,sizeof(log),nullptr,log); glDeleteShader(shader);
        throw std::runtime_error(std::string("Seis3D shader compilation: ")+log);
    }
    return shader;
}

// Link a private program without sharing shader objects or program IDs with the 2-D module.
GLuint createProgram3D(const char* fragmentName) {
    GLuint vertex=0,fragment=0,program=0;
    try {
        vertex=compileShader3D(GL_VERTEX_SHADER,readShader3D("seis3d_slice.vert"));
        fragment=compileShader3D(GL_FRAGMENT_SHADER,readShader3D(fragmentName));
        program=glCreateProgram(); glAttachShader(program,vertex); glAttachShader(program,fragment); glLinkProgram(program);
        GLint ok=0; glGetProgramiv(program,GL_LINK_STATUS,&ok);
        if (!ok) { char log[4096]{}; glGetProgramInfoLog(program,sizeof(log),nullptr,log); throw std::runtime_error(log); }
    } catch (...) { if(program) glDeleteProgram(program); if(vertex) glDeleteShader(vertex); if(fragment) glDeleteShader(fragment); throw; }
    glDetachShader(program,vertex); glDetachShader(program,fragment); glDeleteShader(vertex); glDeleteShader(fragment);
    return program;
}

// Create a bounded texture with edge clamping and linear interpolation for slice display.
void createTexture2D3D(GLuint& texture, int width, int height, GLint format) {
    glGenTextures(1,&texture); glBindTexture(GL_TEXTURE_2D,texture);
    glTexImage2D(GL_TEXTURE_2D,0,format,width,height,0,format==GL_RG32F ? GL_RG : GL_RGBA,GL_FLOAT,nullptr);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
}

// Attach an owned display texture and reject an incomplete off-screen target.
void createFramebuffer3D(GLuint& framebuffer, GLuint texture) {
    glGenFramebuffers(1,&framebuffer); glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)
        throw std::runtime_error("Seis3D framebuffer incomplete.");
}

// Allocate one CUDA-registered staging buffer, three slices and a bounded auxiliary volume.
void initializeGraphics3D(Seis3D_State& s) {
    GLStateScope3D saved;
    // GLEW or the preceding route may leave a diagnostic; isolate our own allocation errors.
    while(glGetError()!=GL_NO_ERROR) {}
    auto& g=s.gl3D; g.layout3D=makeViewLayout3D(s.context3D);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);
    g.sliceProgram3D=createProgram3D("seis3d_slice.frag"); g.volumeProgram3D=createProgram3D("seis3d_volume.frag");
    glGenVertexArrays(1,&g.vao3D);
    for(int p=0;p<3;++p) {
        createTexture2D3D(g.scalarTextures3D[p],g.layout3D.sliceWidth3D[p],g.layout3D.sliceHeight3D[p],GL_RG32F);
        createTexture2D3D(g.displayTextures3D[p],g.layout3D.sliceWidth3D[p],g.layout3D.sliceHeight3D[p],GL_RGBA8);
        createFramebuffer3D(g.framebuffers3D[p],g.displayTextures3D[p]);
    }
    createTexture2D3D(g.volumeDisplayTexture3D,640,640,GL_RGBA8); createFramebuffer3D(g.volumeFramebuffer3D,g.volumeDisplayTexture3D);
    glGenTextures(1,&g.volumeTexture3D); glBindTexture(GL_TEXTURE_3D,g.volumeTexture3D);
    glTexImage3D(GL_TEXTURE_3D,0,GL_RG32F,g.layout3D.volumeX3D,g.layout3D.volumeY3D,g.layout3D.volumeZ3D,0,GL_RG,GL_FLOAT,nullptr);
    glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MIN_FILTER,GL_LINEAR); glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_R,GL_CLAMP_TO_EDGE);
    glGenBuffers(1,&g.stagingBuffer3D); glBindBuffer(GL_PIXEL_UNPACK_BUFFER,g.stagingBuffer3D);
    glBufferData(GL_PIXEL_UNPACK_BUFFER,GLsizeiptr(g.layout3D.totalSamples3D*2*sizeof(float)),nullptr,GL_STREAM_DRAW);
    if (glGetError()!=GL_NO_ERROR) throw std::runtime_error("Seis3D OpenGL allocation failed.");
    requireCuda3D(cudaGraphicsGLRegisterBuffer(&g.interop3D,g.stagingBuffer3D,cudaGraphicsRegisterFlagsWriteDiscard),"register Seis3D staging");
}

// Apply one transfer function to both linked slices and the exploratory volume.
void setTransferUniforms3D(GLuint program,const Seis3D_State& s) {
    const auto& palette=wavePalette3D(s.waveStyle3D);
    glUniform3fv(glGetUniformLocation(program,"palette3D"),5,palette[0].data());
    glUniform1i(glGetUniformLocation(program,"displayMode3D"),s.displayMode3D);
    glUniform1i(glGetUniformLocation(program,"modelStyle3D"),s.modelStyle3D);
    glUniform1i(glGetUniformLocation(program,"unsignedWave3D"),s.component3D==3);
    const float* range=s.materialRangeAuto3D?s.materialRange3D:s.manualMaterialRange3D;
    glUniform2f(glGetUniformLocation(program,"materialRange3D"),range[0],(std::max)(range[0],range[1]));
    glUniform1f(glGetUniformLocation(program,"waveOpacity3D"),s.waveOpacity3D);
    glUniform1f(glGetUniformLocation(program,"waveThreshold3D"),s.waveThreshold3D);
    glUniform1f(glGetUniformLocation(program,"modelOpacity3D"),s.modelOpacity3D);
    glUniform1f(glGetUniformLocation(program,"waveGamma3D"),s.waveGamma3D);
    glUniform1f(glGetUniformLocation(program,"materialBrightness3D"),s.materialBrightness3D);
}

// Update only scalar slices/volume through CUDA-GL interop, with guaranteed unmapping on failure.
void updateGraphics3D(Seis3D_State& s) {
    if (!s.ready3D) return;
    auto& g=s.gl3D;
    const bool extract=s.dirty3D || s.step3D<s.displayedStep3D
        || s.step3D-s.displayedStep3D>=std::uint64_t((std::clamp)(s.displayStride3D,1,16))
        || (!s.running3D && s.step3D!=s.displayedStep3D);
    const std::array<float,32> settings={s.gain3D,s.manualScale3D,float(s.autoScale3D),float(s.displayMode3D),
        float(s.waveStyle3D),float(s.modelStyle3D),float(s.materialRangeAuto3D),s.manualMaterialRange3D[0],
        s.manualMaterialRange3D[1],s.waveOpacity3D,s.waveThreshold3D,s.modelOpacity3D,s.volumeOpacity3D,
        s.volumeThreshold3D,float(s.showVolume3D),s.camera3D.yaw3D,s.camera3D.pitch3D,s.camera3D.distance3D,
        s.camera3D.pan3D[0],s.camera3D.pan3D[1],float(s.component3D),s.waveGamma3D,s.materialBrightness3D};
    if(!extract && g.rendered3D && settings==g.renderedSettings3D) return;
    GLStateScope3D saved;
    if (extract) {
        requireCuda3D(cudaGraphicsMapResources(1,&g.interop3D,s.gpu3D.stream3D),"map Seis3D views");
        struct Mapping3D {
            cudaGraphicsResource_t* resource3D; cudaStream_t stream3D; bool mapped3D=true;
            // Ensure exceptions cannot leave the GL staging buffer mapped to CUDA.
            ~Mapping3D() { if(mapped3D) cudaGraphicsUnmapResources(1,resource3D,stream3D); }
        } mapping{&g.interop3D,s.gpu3D.stream3D};
        float* output=nullptr; std::size_t capacity=0;
        requireCuda3D(cudaGraphicsResourceGetMappedPointer(reinterpret_cast<void**>(&output),&capacity,g.interop3D),"get Seis3D staging pointer");
        if (capacity<g.layout3D.totalSamples3D*2*sizeof(float)) throw std::runtime_error("Seis3D staging capacity mismatch.");
        extractViews3D(s.gpu3D,s.context3D,output,g.layout3D,s.slice3D,s.component3D,s.materialComponent3D,s.showVolume3D,s.viewMaximum3D,s.materialRange3D);
        requireCuda3D(cudaGraphicsUnmapResources(1,&g.interop3D,s.gpu3D.stream3D),"unmap Seis3D views"); mapping.mapped3D=false;
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER,g.stagingBuffer3D);
        for(int p=0;p<3;++p) {
            glBindTexture(GL_TEXTURE_2D,g.scalarTextures3D[p]);
            glTexSubImage2D(GL_TEXTURE_2D,0,0,0,g.layout3D.sliceWidth3D[p],g.layout3D.sliceHeight3D[p],GL_RG,GL_FLOAT,
                reinterpret_cast<void*>(g.layout3D.sliceOffset3D[p]*2*sizeof(float)));
        }
        if(s.showVolume3D) {
            glBindTexture(GL_TEXTURE_3D,g.volumeTexture3D);
            glTexSubImage3D(GL_TEXTURE_3D,0,0,0,0,g.layout3D.volumeX3D,g.layout3D.volumeY3D,g.layout3D.volumeZ3D,GL_RG,GL_FLOAT,
                reinterpret_cast<void*>(g.layout3D.volumeOffset3D*2*sizeof(float)));
        }
        s.dirty3D=false; s.displayedStep3D=s.step3D;
    }
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0); glBindVertexArray(g.vao3D);
    const float scale=s.autoScale3D ? (std::max)(s.viewMaximum3D,1.0e-12f) : s.manualScale3D;
    glUseProgram(g.sliceProgram3D);
    setTransferUniforms3D(g.sliceProgram3D,s);
    glUniform1i(glGetUniformLocation(g.sliceProgram3D,"sliceTexture3D"),0);
    glUniform1f(glGetUniformLocation(g.sliceProgram3D,"scale3D"),scale);
    glUniform1f(glGetUniformLocation(g.sliceProgram3D,"gain3D"),s.gain3D);
    glUniform1i(glGetUniformLocation(g.sliceProgram3D,"component3D"),s.component3D);
    for(int p=0;p<3;++p) {
        glBindFramebuffer(GL_FRAMEBUFFER,g.framebuffers3D[p]); glViewport(0,0,g.layout3D.sliceWidth3D[p],g.layout3D.sliceHeight3D[p]);
        glBindTexture(GL_TEXTURE_2D,g.scalarTextures3D[p]); glDrawArrays(GL_TRIANGLES,0,3);
    }
    if(s.showVolume3D) {
        const auto program=g.volumeProgram3D;
        glUseProgram(program); setTransferUniforms3D(program,s); glBindFramebuffer(GL_FRAMEBUFFER,g.volumeFramebuffer3D); glViewport(0,0,640,640);
        glBindTexture(GL_TEXTURE_3D,g.volumeTexture3D);
        glUniform1i(glGetUniformLocation(program,"volumeTexture3D"),0);
        const float extent=float((std::max)({s.context3D.nx3D,s.context3D.ny3D,s.context3D.nz3D}));
        glUniform3f(glGetUniformLocation(program,"extent3D"),s.context3D.nx3D/extent,s.context3D.nz3D/extent,s.context3D.ny3D/extent);
        glUniform2fv(glGetUniformLocation(program,"pan3D"),1,s.camera3D.pan3D);
        glUniform1f(glGetUniformLocation(program,"yaw3D"),s.camera3D.yaw3D);
        glUniform1f(glGetUniformLocation(program,"pitch3D"),s.camera3D.pitch3D);
        glUniform1f(glGetUniformLocation(program,"distance3D"),s.camera3D.distance3D);
        glUniform1f(glGetUniformLocation(program,"scale3D"),scale); glUniform1f(glGetUniformLocation(program,"gain3D"),s.gain3D);
        glUniform1f(glGetUniformLocation(program,"opacity3D"),s.volumeOpacity3D);
        glUniform1f(glGetUniformLocation(program,"threshold3D"),s.volumeThreshold3D); glDrawArrays(GL_TRIANGLES,0,3);
    }
    if(glGetError()!=GL_NO_ERROR) throw std::runtime_error("Seis3D texture upload or offscreen draw failed. Rebuild the application and check GPU diagnostics.");
    g.renderedSettings3D=settings;g.rendered3D=true;
}


} // namespace Seis3D::Detail3D

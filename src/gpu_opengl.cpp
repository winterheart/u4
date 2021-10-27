/*
  XU4 OpenGL Renderer
  Copyright 2021 Karl Robillard

  This file is part of XU4.

  XU4 is free software: you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  XU4 is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with XU4.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "tileset.h"
#include "u4.h"     // VIEWPORT_W

extern uint32_t getTicks();

//#include "gpu_opengl.h"

#define dprint  printf

#define LOC_POS     0
#define LOC_UV      1

const char* cmap_vertShader =
    "#version 330\n"
    "uniform mat4 transform;\n"
    "layout(location = 0) in vec3 position;\n"
    "layout(location = 1) in vec4 uv;\n"
    "out vec4 texCoord;\n"
    "void main() {\n"
    "  texCoord = uv;\n"
    "  gl_Position = transform * vec4(position, 1.0);\n"
    "}\n";

const char* cmap_fragShader =
    "#version 330\n"
    "uniform sampler2D cmap;\n"
    "uniform sampler2D mmap;\n"
    "uniform vec4 tint;\n"
    "uniform vec2 scroll;\n"
    "in vec4 texCoord;\n"
    "out vec4 fragColor;\n"
    "void main() {\n"
    "  vec4 texel;\n"
    "  vec4 material = texture(mmap, texCoord.st);\n"
    "  if (material.b > 0.95) {\n"
    "    vec2 tc = texCoord.sq; \n"
    "    float nv = texCoord.p - scroll.t * 0.3;\n"
    "    tc.t += (nv - floor(nv)) * scroll.s;\n"
    "    texel = texture(cmap, tc);\n"
    "/*\n"
    "    float nv = texCoord.p + scroll.t;\n"
    "    texel = vec4(vec3(nv - floor(nv)), 1.0);\n"
    "*/\n"
    "  } else {\n"
    "    texel = texture(cmap, texCoord.st);\n"
    "  }\n"
    "  fragColor = tint * texel;\n"
    "}\n";

const char* world_vertShader =
    "#version 330\n"
    "uniform mat4 transform;\n"
    "layout(location = 0) in vec3 position;\n"
    "layout(location = 1) in vec4 uv;\n"
    "out vec4 texCoord;\n"
    "out vec2 shadowCoord;\n"
    "void main() {\n"
    "  texCoord = uv;\n"
    "  gl_Position = transform * vec4(position, 1.0);\n"
    "  shadowCoord = (gl_Position.xy + 1.0) * 0.5;\n"
    "}\n";

const char* world_fragShader =
    "#version 330\n"
    "uniform sampler2D cmap;\n"
    "uniform sampler2D mmap;\n"
    "uniform sampler2D shadowMap;\n"
    "uniform vec2 scroll;\n"
    "in vec4 texCoord;\n"
    "in vec2 shadowCoord;\n"
    "out vec4 fragColor;\n"
    "void main() {\n"
    "  vec4 texel;\n"
    "  vec4 material = texture(mmap, texCoord.st);\n"
    "  if (material.b > 0.95) {\n"
    "    vec2 tc = texCoord.sq; \n"
    "    float nv = texCoord.p - scroll.t * 0.3;\n"
    "    tc.t += (nv - floor(nv)) * scroll.s;\n"
    "    texel = texture(cmap, tc);\n"
    "/*\n"
    "    float nv = texCoord.p + scroll.t;\n"
    "    texel = vec4(vec3(nv - floor(nv)), 1.0);\n"
    "*/\n"
    "  } else {\n"
    "    texel = texture(cmap, texCoord.st);\n"
    "  }\n"
    "  vec4 shade = texture(shadowMap, shadowCoord);\n"
    "  fragColor = vec4(shade.aaa, 1.0) * texel;\n"
    "}\n";

#define MAT_X 12
#define MAT_Y 13
static const float unitMatrix[16] = {
    1.0, 0.0, 0.0, 0.0,
    0.0, 1.0, 0.0, 0.0,
    0.0, 0.0, 1.0, 0.0,
    0.0, 0.0, 0.0, 1.0
};

#define ATTR_COUNT      7
#define ATTR_STRIDE     (sizeof(float) * ATTR_COUNT)
static const float quadAttr[] = {
    // X   Y   Z       U  V  vunit  scrollSourceV
   -1.0,-1.0, 0.0,   0.0, 1.0, 0.0, 0.0,
    1.0,-1.0, 0.0,   1.0, 1.0, 0.0, 0.0,
    1.0, 1.0, 0.0,   1.0, 0.0, 0.0, 0.0,
    1.0, 1.0, 0.0,   1.0, 0.0, 0.0, 0.0,
   -1.0, 1.0, 0.0,   0.0, 0.0, 0.0, 0.0,
   -1.0,-1.0, 0.0,   0.0, 1.0, 0.0, 0.0
};

#define SHADOW_DIM      512


#ifdef _WIN32
#include "glad.c"
#endif

//#define DEBUG_GL
#ifdef DEBUG_GL
void _debugGL( GLenum source, GLenum type, GLuint id, GLenum severity,
               GLsizei length, const GLchar* message, const void* userParam )
{
    (void) severity;
    (void) length;
    (void) userParam;

    fprintf(stderr, "GL DEBUG %d:%s 0x%x %s\n",
            source,
            (type == GL_DEBUG_TYPE_ERROR) ? " ERROR" : "",
            id, message );
}

static void enableGLDebug()
{
    // Requires GL_KHR_debug extension
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageControl(GL_DEBUG_SOURCE_API, GL_DONT_CARE,
                          GL_DEBUG_SEVERITY_NOTIFICATION, 0, NULL, GL_FALSE);
    glDebugMessageCallback(_debugGL, NULL);
}
#endif

static void printInfoLog(GLuint obj, int prog)
{
    GLint infologLength;
    GLint charsWritten;
    char* infoLog;

    if( prog )
        glGetProgramiv( obj, GL_INFO_LOG_LENGTH, &infologLength );
    else
        glGetShaderiv( obj, GL_INFO_LOG_LENGTH, &infologLength );

    if( infologLength > 0 ) {
        infoLog = (char*) malloc( infologLength );

        if( prog )
            glGetProgramInfoLog( obj, infologLength, &charsWritten, infoLog );
        else
            glGetShaderInfoLog( obj, infologLength, &charsWritten, infoLog );

        fprintf(stderr, "%s\n", infoLog);
        free( infoLog );
    } else {
        fprintf(stderr, "%s failed\n", prog ? "glLinkProgram"
                                            : "glCompileShader");
    }
}

/*
 * Returns zero on success or 1-3 to indicate compile/link error.
 */
static int compileShaderParts(GLuint program, const char** src, int vcount,
                              int fcount)
{
    GLint ok;
    GLuint vobj = glCreateShader(GL_VERTEX_SHADER);
    GLuint fobj = glCreateShader(GL_FRAGMENT_SHADER);

    glShaderSource(vobj, vcount, (const GLchar**) src, NULL);
    glCompileShader(vobj);
    glGetShaderiv(vobj, GL_COMPILE_STATUS, &ok);
    if (! ok) {
        printInfoLog(vobj, 0);
        return 1;
    }
    src += vcount;

    glShaderSource(fobj, fcount, (const GLchar**) src, NULL);
    glCompileShader(fobj);
    glGetShaderiv(fobj, GL_COMPILE_STATUS, &ok);
    if (! ok) {
        printInfoLog(fobj, 0);
        return 2;
    }

    glAttachShader(program, vobj);
    glAttachShader(program, fobj);
    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (! ok)
        printInfoLog(program, 1);

    // These will actually go away when the program is deleted.
    glDeleteShader(vobj);
    glDeleteShader(fobj);

    return ok ? 0 : 3;
}

static int compileShaders(GLuint program, const char* vert, const char* frag)
{
    const char* src[2];
    src[0] = vert;
    src[1] = frag;
    return compileShaderParts(program, src, 1, 1);
}

static char* readShader(const char* filename)
{
#ifdef CONF_MODULE
    const CDIEntry* ent = xu4.config->fileEntry(filename);
    if (! ent)
        return NULL;

    char* buf = (char*) malloc(ent->bytes + 1);
    if (buf) {
        FILE* fp = fopen(xu4.config->modulePath(), "rb");
        if (fp) {
            fseek(fp, ent->offset, SEEK_SET);

            size_t len = fread(buf, 1, ent->bytes, fp);
            fclose(fp);
            if (len == ent->bytes) {
                buf[len] = '\0';
                return buf;
            }
        }
        free(buf);
    }
#else
    char fnBuf[40];
    const size_t bsize = 4096;
    char* buf = (char*) malloc(bsize);
    if (buf) {
        strcpy(fnBuf, "graphics/shader/");
        strcat(fnBuf, filename);

        FILE* fp = fopen(fnBuf, "rb");
        if (fp) {
            size_t len = fread(buf, 1, bsize-1, fp);
            fclose(fp);
            if (len > 16) {
                buf[len] = '\0';
                return buf;
            }
        }
        free(buf);
    }
#endif
    return NULL;
}

/*
 * Returns zero on success or 1-4 to indicate compile/link/read error.
 */
static int compileSLFile(GLuint program, const char* filename, int scale)
{
    const char* src[4];
    int res = 4;
    char* buf = readShader(filename);

    if (buf) {
        if (scale > 2) {
            char* spos = strstr(buf, "SCALE 2");
            if (spos)
                spos[6] = '0' + scale;
        }

        src[0] = "#version 330\n#define VERTEX\n";
        src[1] = buf;
        src[2] = "#version 330\n#define FRAGMENT\n";
        src[3] = buf;

        res = compileShaderParts(program, src, 2, 2);
        free(buf);
    }
    return res;
}

static void _defineAttributeLayout(GLuint vao, GLuint vbo)
{
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glEnableVertexAttribArray(LOC_POS);
    glVertexAttribPointer(LOC_POS, 3, GL_FLOAT, GL_FALSE, ATTR_STRIDE, 0);
    glEnableVertexAttribArray(LOC_UV);
    glVertexAttribPointer(LOC_UV,  4, GL_FLOAT, GL_FALSE, ATTR_STRIDE,
                          (const GLvoid*) 12);
}

static GLuint _makeFramebuffer(GLuint texId)
{
    GLuint fbo;
    GLenum status;

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, texId, 0);

    status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "Framebuffer invalid: 0x%04X\n", status);
        return 0;
    }
    return fbo;
}

extern Image* loadImage_png(U4FILE *file);
uint32_t gpu_makeTexture(const Image32* img);

static U4FILE* openHQXTableImage(int scale)
{
#ifdef CONF_MODULE
    char lutFile[16];
    strcpy(lutFile, "hq2x.png");
    lutFile[2] = '0' + scale;

    const CDIEntry* ent = xu4.config->fileEntry(lutFile);
    if (! ent)
        return NULL;

    U4FILE* uf = u4fopen_stdio(xu4.config->modulePath());
    u4fseek(uf, ent->offset, SEEK_SET);
    return uf;
#else
    char lutFile[32];
    strcpy(lutFile, "graphics/shader/hq2x.png");
    lutFile[18] = '0' + scale;
    return u4fopen_stdio(lutFile);
#endif
}

/*
 * Define 2D texture storage.
 *
 * \param data  Pointer to RGBA uint8_t pixels or NULL.
 */
static void gpu_defineTex(GLuint tex, int w, int h, const void* data,
                          GLint internalFormat, GLenum filter)
{
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, w, h,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
}

static void reserveDrawList(const GLuint* vbo, int byteSize)
{
    int i;
    for (i = 0; i < 2; ++i) {
        glBindBuffer(GL_ARRAY_BUFFER, vbo[i]);
        glBufferData(GL_ARRAY_BUFFER, byteSize, NULL, GL_DYNAMIC_DRAW);
    }
}

bool gpu_init(void* res, int w, int h, int scale)
{
    OpenGLResources* gr = (OpenGLResources*) res;
    GLuint sh;
    GLint cmap, mmap;

    assert(sizeof(GLuint) == sizeof(uint32_t));

    memset(gr, 0, sizeof(OpenGLResources));
    /*
    gr->scalerLut = 0;
    gr->scaler = 0;
    gr->blockCount = 0;
    gr->tilesTex = 0;
    */
    gr->dl[0].buf = GLOB_DRAW_LIST0;
    gr->dl[0].byteSize = ATTR_STRIDE * 6 * 400;
    gr->dl[1].buf = GLOB_FX_LIST0;
    gr->dl[1].byteSize = ATTR_STRIDE * 6 * 20;

#ifdef DEBUG_GL
    enableGLDebug();
#endif

    // Create screen & shadow textures.
    glGenTextures(2, &gr->screenTex);
    gpu_defineTex(gr->screenTex, 320, 200, NULL, GL_RGB, GL_NEAREST);
    gpu_defineTex(gr->shadowTex, SHADOW_DIM, SHADOW_DIM, NULL,
                  GL_RGBA, GL_LINEAR);

    gr->shadowFbo = _makeFramebuffer(gr->shadowTex);
    if (! gr->shadowFbo)
        return false;
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);


    // Set default state.
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glViewport(0, 0, w, h);


    // Create scaler shader.
    if (scale > 1) {
        if (scale > 4)
            scale = 4;

        U4FILE* uf = openHQXTableImage(scale);
        if (uf) {
            Image* img = loadImage_png(uf);
            u4fclose(uf);
            if (img) {
                gr->scalerLut = gpu_makeTexture(img);
                delete img;
            }
        }
        if (! gr->scalerLut)
            return false;

        gr->scaler = sh = glCreateProgram();
        if (compileSLFile(sh, "hq2x.glsl", scale))
            return false;

        gr->slocScMat = glGetUniformLocation(sh, "MVPMatrix");
        gr->slocScDim = glGetUniformLocation(sh, "TextureSize");
        gr->slocScTex = glGetUniformLocation(sh, "Texture");
        gr->slocScLut = glGetUniformLocation(sh, "LUT");

        glUseProgram(sh);
        glUniformMatrix4fv(gr->slocScMat, 1, GL_FALSE, unitMatrix);
        glUniform2f(gr->slocScDim, (float) (w / scale), (float) (h / scale));
        glUniform1i(gr->slocScTex, GTU_CMAP);
        glUniform1i(gr->slocScLut, GTU_SCALER_LUT);
    }


    // Create shadowcast shader.
    gr->shadow = sh = glCreateProgram();
    if (compileSLFile(sh, "shadowcast.glsl", 0))
        return false;

    gr->shadowTrans  = glGetUniformLocation(sh, "transform");
    gr->shadowVport  = glGetUniformLocation(sh, "vport");
    gr->shadowViewer = glGetUniformLocation(sh, "viewer");
    gr->shadowCounts = glGetUniformLocation(sh, "shape_count");
    gr->shadowShapes = glGetUniformLocation(sh, "shapes");


    // Create colormap shader.
    gr->shadeColor = sh = glCreateProgram();
    if (compileShaders(sh, cmap_vertShader, cmap_fragShader))
        return false;

    gr->slocTrans   = glGetUniformLocation(sh, "transform");
    cmap            = glGetUniformLocation(sh, "cmap");
    mmap            = glGetUniformLocation(sh, "mmap");
    gr->slocTint    = glGetUniformLocation(sh, "tint");
    gr->slocScroll  = glGetUniformLocation(sh, "scroll");

    glUseProgram(sh);
    glUniformMatrix4fv(gr->slocTrans, 1, GL_FALSE, unitMatrix);
    glUniform1i(cmap, GTU_CMAP);
    glUniform1i(mmap, GTU_MATERIAL);
    glUniform4f(gr->slocTint, 1.0, 1.0, 1.0, 1.0);


    // Create world shader.
    gr->shadeWorld = sh = glCreateProgram();
    if (compileShaders(sh, world_vertShader, world_fragShader))
        return false;

    gr->worldTrans     = glGetUniformLocation(sh, "transform");
    cmap               = glGetUniformLocation(sh, "cmap");
    mmap               = glGetUniformLocation(sh, "mmap");
    gr->worldShadowMap = glGetUniformLocation(sh, "shadowMap");
    gr->worldScroll    = glGetUniformLocation(sh, "scroll");

    glUseProgram(sh);
    glUniformMatrix4fv(gr->worldTrans, 1, GL_FALSE, unitMatrix);
    glUniform1i(cmap, GTU_CMAP);
    glUniform1i(mmap, GTU_MATERIAL);
    glUniform1i(gr->worldShadowMap, GTU_SHADOW);


    // Create our vertex buffers.
    glGenBuffers(GLOB_COUNT, gr->vbo);

    // Reserve space in the double-buffered draw lists.
    reserveDrawList(gr->vbo + GLOB_DRAW_LIST0, gr->dl[0].byteSize);
    reserveDrawList(gr->vbo + GLOB_FX_LIST0,   gr->dl[1].byteSize);

    // Create quad geometry.
    glBindBuffer(GL_ARRAY_BUFFER, gr->vbo[GLOB_QUAD]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadAttr), quadAttr, GL_STATIC_DRAW);

    // Create vertex attribute layouts.
    glGenVertexArrays(GLOB_COUNT, gr->vao);
    for(int i = 0; i < GLOB_COUNT; ++i)
        _defineAttributeLayout(gr->vao[i], gr->vbo[i]);
    glBindVertexArray(0);

    return true;
}

void gpu_free(void* res)
{
    OpenGLResources* gr = (OpenGLResources*) res;

    if (gr->scaler) {
        glDeleteProgram(gr->scaler);
        glDeleteTextures(1, &gr->scalerLut);
    }

    glDeleteVertexArrays(GLOB_COUNT, gr->vao);
    glDeleteBuffers(GLOB_COUNT, gr->vbo);
    glDeleteProgram(gr->shadeColor);
    glDeleteProgram(gr->shadeWorld);
    glDeleteProgram(gr->shadow);
    glDeleteFramebuffers(1, &gr->shadowFbo);
    glDeleteTextures(2, &gr->screenTex);
}

void gpu_viewport(int x, int y, int w, int h)
{
    glViewport(x, y, w, h);
}

uint32_t gpu_makeTexture(const Image32* img)
{
    GLuint tex;
    glGenTextures(1, &tex);
    gpu_defineTex(tex, img->w, img->h, img->pixels, GL_RGBA, GL_NEAREST);
    return tex;
}

void gpu_blitTexture(uint32_t tex, int x, int y, const Image32* img)
{
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, img->w, img->h,
                    GL_RGBA, GL_UNSIGNED_BYTE, img->pixels);
}

void gpu_freeTexture(uint32_t tex)
{
    glDeleteTextures(1, &tex);
}

/*
 * Return the identifier of the screen texture which is created by gpu_init().
 */
uint32_t gpu_screenTexture(void* res)
{
    OpenGLResources* gr = (OpenGLResources*) res;
    return gr->screenTex;
}

void gpu_setTilesTexture(void* res, uint32_t tex, uint32_t mat, float vDim)
{
    OpenGLResources* gr = (OpenGLResources*) res;
    gr->tilesTex = tex;
    gr->tilesMat = mat;
    gr->tilesVDim = vDim;
}

/*
 * Render a background image using the scale defined with gpu_init().
 */
void gpu_drawTextureScaled(void* res, uint32_t tex)
{
    OpenGLResources* gr = (OpenGLResources*) res;

    glActiveTexture(GL_TEXTURE0 + GTU_CMAP);
    glBindTexture(GL_TEXTURE_2D, tex);

    if (gr->scaler) {
        glUseProgram(gr->scaler);
        glActiveTexture(GL_TEXTURE0 + GTU_SCALER_LUT);
        glBindTexture(GL_TEXTURE_2D, gr->scalerLut);
    } else {
        glUseProgram(gr->shadeColor);
        glUniformMatrix4fv(gr->slocTrans, 1, GL_FALSE, unitMatrix);
    }

    glDisable(GL_BLEND);
    glBindVertexArray(gr->vao[ GLOB_QUAD ]);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

/*
 * Begin a rendered frame cleared to a solid color.
 */
void gpu_clear(void* res, const float* color)
{
    glClearColor(color[0], color[1], color[2], color[3]);
    glClear(GL_COLOR_BUFFER_BIT);
}

/*
 * Begin adding triangles to a double-buffered draw list.
 *
 * Returns a pointer to the start of the attributes buffer.
 * This should be advanced and passed to gpu_endTris() when all triangles
 * have been generated.
 */
float* gpu_beginTris(void* res, int list)
{
    OpenGLResources* gr = (OpenGLResources*) res;
    DrawList* dl = gr->dl + list;

    dl->buf ^= 1;
    glBindBuffer(GL_ARRAY_BUFFER, gr->vbo[ dl->buf ]);
    gr->dptr = (GLfloat*) glMapBufferRange(GL_ARRAY_BUFFER, 0, dl->byteSize,
                                           GL_MAP_WRITE_BIT);
    return gr->dptr;
}

/*
 * Complete the list of triangles generated since gpu_beginTris().
 * Each call to gpu_beginTris() must be paired with gpu_endTris().
 * Each list must be created separately; these calls cannot be interleaved.
 */
void gpu_endTris(void* res, int list, float* attr)
{
    OpenGLResources* gr = (OpenGLResources*) res;

    glUnmapBuffer(GL_ARRAY_BUFFER);

    assert(gr->dptr);
    gr->dl[ list ].count = attr - gr->dptr;
    gr->dptr = NULL;
}

/*
 * Draw any triangles created between the last gpu_beginTris/endTris calls.
 */
void gpu_drawTris(void* res, int list)
{
    OpenGLResources* gr = (OpenGLResources*) res;
    DrawList* dl = gr->dl + list;

    if (! dl->count)
        return;
    //dprint("gpu_drawTris %d\n", dl->count);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBlendEquation(GL_FUNC_ADD);

    glUniformMatrix4fv(gr->slocTrans, 1, GL_FALSE, unitMatrix);
    glBindVertexArray(gr->vao[ dl->buf ]);
    glDrawArrays(GL_TRIANGLES, 0, dl->count / ATTR_COUNT);
}

float* gpu_emitQuad(float* attr, const float* drawRect, const float* uvRect)
{
    float w = drawRect[2];
    float h = drawRect[3];
    int i;

    /*
   -1.0,-1.0, 0.0,   0.0, 1.0,
    1.0,-1.0, 0.0,   1.0, 1.0,
    1.0, 1.0, 0.0,   1.0, 0.0,
    1.0, 1.0, 0.0,   1.0, 0.0,
   -1.0, 1.0, 0.0,   0.0, 0.0,
   -1.0,-1.0, 0.0,   0.0, 1.0
    */

#if 0
    dprint( "gpu_emitQuad %f,%f,%f,%f  %f,%f,%f,%f\n",
            drawRect[0], drawRect[1], drawRect[2], drawRect[3],
            uvRect[0], uvRect[1], uvRect[2], uvRect[3]);
#endif

#define EMIT_POS(x,y) \
    *attr++ = x; \
    *attr++ = y; \
    *attr++ = 0.0f

#define EMIT_UV(u,v) \
    *attr++ = u; \
    *attr++ = v; \
    *attr++ = 0.0f; \
    *attr++ = 0.0f

    // NOTE: We only do writes to attr here (avoid memcpy).

    // First vertex, lower-left corner
    EMIT_POS(drawRect[0], drawRect[1]);
    EMIT_UV(uvRect[0], uvRect[3]);

    // Lower-right corner
    EMIT_POS(drawRect[0] + w, drawRect[1]);
    EMIT_UV(uvRect[2], uvRect[3]);

    // Top-right corner
    for (i = 0; i < 2; ++i) {
        EMIT_POS(drawRect[0] + w, drawRect[1] + h);
        EMIT_UV(uvRect[2], uvRect[1]);
    }

    // Top-left corner
    EMIT_POS(drawRect[0], drawRect[1] + h);
    EMIT_UV(uvRect[0], uvRect[1]);

    // Repeat first vertex
    EMIT_POS(drawRect[0], drawRect[1]);
    EMIT_UV(uvRect[0], uvRect[3]);

    return attr;
}

float* gpu_emitQuadScroll(float* attr, const float* drawRect,
                          const float* uvRect, float scrollSourceV)
{
    float w = drawRect[2];
    float h = drawRect[3];
    int i;

#define EMIT_UVS(u,v,vunit) \
    *attr++ = u; \
    *attr++ = v; \
    *attr++ = vunit; \
    *attr++ = scrollSourceV

    // NOTE: We only do writes to attr here (avoid memcpy).

    // First vertex, lower-left corner
    EMIT_POS(drawRect[0], drawRect[1]);
    EMIT_UVS(uvRect[0], uvRect[3], 1.0f);

    // Lower-right corner
    EMIT_POS(drawRect[0] + w, drawRect[1]);
    EMIT_UVS(uvRect[2], uvRect[3], 1.0f);

    // Top-right corner
    for (i = 0; i < 2; ++i) {
        EMIT_POS(drawRect[0] + w, drawRect[1] + h);
        EMIT_UVS(uvRect[2], uvRect[1], 0.0f);
    }

    // Top-left corner
    EMIT_POS(drawRect[0], drawRect[1] + h);
    EMIT_UVS(uvRect[0], uvRect[1], 0.0f);

    // Repeat first vertex
    EMIT_POS(drawRect[0], drawRect[1]);
    EMIT_UVS(uvRect[0], uvRect[3], 1.0f);

    return attr;
}

//--------------------------------------
// Map Rendering

void gpu_resetMap(void* res, const Map* map)
{
    OpenGLResources* gr = (OpenGLResources*) res;

    gr->blockCount = 0;

    // Initialize map chunks.
    assert(map->chunk_height == map->chunk_width);
    gr->mapChunkDim = map->chunk_width;
    gr->mapChunkVertCount = gr->mapChunkDim * gr->mapChunkDim * 6;

    for (int i = 0; i < 4; ++i) {
        glBindBuffer(GL_ARRAY_BUFFER, gr->vbo[ GLOB_MAP_CHUNK0+i ]);
        glBufferData(GL_ARRAY_BUFFER, gr->mapChunkVertCount * ATTR_STRIDE,
                     NULL, GL_DYNAMIC_DRAW);
    }

    // Clear chunk cache.
    memset(gr->mapChunkId, 0xff, 4*sizeof(uint16_t));
}

#define VIEW_TILE_SIZE  (2.0f / VIEWPORT_W)

/*
 * \param chunk    Map data aligned at top-left of chunk.
 * \param dim      Chunk tile dimensions
 * \param stride   Chunk stride (map tile width)
 */
static void _buildChunkGeo(GLuint vbo, const TileId* chunk, int dim,
                           int stride, const TileRenderData* renderData,
                           const float* uvTable)
{
    float drawRect[4];  // x, y, width, height
    const float* uvCur;
    const float* uvScroll;
    float* attr;
    const TileId* ip;
    const TileRenderData* tr;
    float startX;
    int x, y;
    int vcount = dim * dim * 6;

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    attr = (float*) glMapBufferRange(GL_ARRAY_BUFFER, 0, vcount * ATTR_STRIDE,
                                     GL_MAP_WRITE_BIT);
    if (! attr) {
        fprintf(stderr, "buildChunkGeo: glMapBufferRange failed\n");
        return;
    }

    // Placing center of the top left tile at the origin.
    startX = -0.5f * VIEW_TILE_SIZE;
    drawRect[1] = startX;
    drawRect[2] = VIEW_TILE_SIZE;
    drawRect[3] = VIEW_TILE_SIZE;

    for (y = 0; y < dim; ++y) {
        drawRect[0] = startX;
        ip = chunk;
        for (x = 0; x < dim; ++x) {
            tr = renderData + *ip++;
            uvCur = uvTable + tr->vid*4;
            if (tr->scroll != VID_UNSET) {
                uvScroll = uvTable + tr->scroll*4;
                attr = gpu_emitQuadScroll(attr, drawRect, uvCur, uvScroll[1]);
            } else
                attr = gpu_emitQuad(attr, drawRect, uvCur);
            drawRect[0] += drawRect[2];
        }
        drawRect[1] -= drawRect[3];
        chunk += stride;
    }

    glUnmapBuffer(GL_ARRAY_BUFFER);
}

struct ChunkLoc {
    int16_t x, y;
};

struct ChunkInfo {
    const TileId* mapData;
    const TileRenderData* renderData;
    const float* uvs;
    const GLuint* vbo;
    uint16_t* mapChunkId;
    ChunkLoc* chunkLoc;
    int mapW;
    int mapH;
    int cdim;
    int geoUsedMask;
};

#define WRAP(x,loc,limit) \
    if (x < 0) { \
        x += limit; \
        loc = -limit; \
    } else if (x >= limit) { \
        x -= limit; \
        loc = limit; \
    } else \
        loc = 0;

// LIMIT: Maximum of 256x256 chunks.
#define CHUNK_ID(c,r)       (c<<8 | r)

#define CHUNK_CACHE_SIZE    4

/*
 * Find (or create) chunk geometry in the four reserved VBO slots.
 * Return the chunk vertex buffer used (0-3), or -1 if no slot is available.
 *
 * \param x         Map tile column.
 * \param y         Map tile row.
 * \param bumpPass  Replace a cached chunk.
 */
static int _obtainChunkGeo(ChunkInfo* ci, int x, int y, int bumpPass)
{
    int i;
    int ccol, crow;
    int wx, wy;
    uint16_t chunkId;
    ChunkLoc* loc;

    WRAP(x, wx, ci->mapW);
    WRAP(y, wy, ci->mapH);
    ccol = x / ci->cdim;
    crow = y / ci->cdim;
    chunkId = CHUNK_ID(ccol, crow);

    if (bumpPass)
    {
        // Find unassigned chunk to replace.
        for (i = 0; i < CHUNK_CACHE_SIZE; ++i) {
            if ((ci->geoUsedMask & (1 << i)) == 0)
                goto build;
        }
        // This should never be reached unless the function is called more
        // than CHUNK_CACHE_SIZE times.
        assert(0 && "bumpPass failed");
        return -1;
    }
    else
    {
        // Check if already made.
        for (i = 0; i < CHUNK_CACHE_SIZE; ++i) {
            if (ci->mapChunkId[i] == chunkId)
                goto used;
        }

        // Find unused VBO.
        for (i = 0; i < CHUNK_CACHE_SIZE; ++i) {
            if (ci->mapChunkId[i] == 0xffff)
                goto build;
        }

        // No unused VBO, fail.
        return -1;
    }

build:
    ci->mapChunkId[i] = chunkId;
    _buildChunkGeo(ci->vbo[i],
                   ci->mapData + (crow * ci->mapW + ccol) * ci->cdim,
                   ci->cdim, ci->mapW, ci->renderData, ci->uvs);
used:
    loc = ci->chunkLoc + i;
    loc->x = wx + (ccol * ci->cdim);
    loc->y = wy + (crow * ci->cdim);
    ci->geoUsedMask |= 1 << i;
    return i;
}

/*
 * \param map           Pointer to map.
 * \param tileUVs       Table of four floats (minU,minV,maxU,maxV) per tile.
 * \param blocks        Sets the occluder shapes for shadowcasting.
 *                      Pass NULL to reuse any previously set shapes.
 * \param cx            Map tile row to center view on.
 * \param cy            Map tile column to center view on.
 * \param viewRadius    Number of tiles (horiz & vert) to draw around cx,cy.
 */
void gpu_drawMap(void* res, const Map* map, const float* tileUVs,
                 const BlockingGroups* blocks,
                 int cx, int cy, int viewRadius)
{
    OpenGLResources* gr = (OpenGLResources*) res;
    ChunkLoc cloc[4];   // Tile location of chunks on the map.
    int i, usedMask;

#if 1
    // Render shadows.
    if (blocks)
        gr->blockCount = blocks->left + blocks->center + blocks->right;

    if (gr->blockCount) {
        GLint vp[4];
        glGetIntegerv(GL_VIEWPORT, vp);

        glUseProgram(gr->shadow);

        if (blocks) {
            glUniformMatrix4fv(gr->shadowTrans, 1, GL_FALSE, unitMatrix);
            glUniform4f(gr->shadowVport, 0.0f, 0.0f, SHADOW_DIM, SHADOW_DIM);
            glUniform3f(gr->shadowViewer, 0.0f, 0.0f, 11.0f);
            glUniform3i(gr->shadowCounts, blocks->left, blocks->center,
                                          blocks->right);
            glUniform3fv(gr->shadowShapes, gr->blockCount, blocks->tilePos);
        }

        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, gr->shadowFbo);
        glViewport(0, 0, SHADOW_DIM, SHADOW_DIM);

        glDisable(GL_BLEND);
        glBindVertexArray(gr->vao[ GLOB_QUAD ]);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glViewport(vp[0], vp[1], vp[2], vp[3]);
    }
#endif

    {
    ChunkInfo ci;
    int bindex[4];  // Chunk vertex buffer index (0-3) at view corner.
    int left, top, right, bot;

    ci.mapData = map->data;
    ci.renderData = map->tileset->render;
    ci.uvs    = tileUVs;
    ci.vbo    = gr->vbo + GLOB_MAP_CHUNK0;
    ci.mapChunkId = gr->mapChunkId;
    ci.chunkLoc = cloc;
    ci.mapW   = map->width;
    ci.mapH   = map->height;
    ci.cdim   = gr->mapChunkDim;
    ci.geoUsedMask  = 0;

    left  = cx - viewRadius;
    top   = cy - viewRadius;
    right = cx + viewRadius;
    bot   = cy + viewRadius;

    // First pass to see what chunks are cached.
    bindex[0] = _obtainChunkGeo(&ci, left,  top, 0);
    bindex[1] = _obtainChunkGeo(&ci, right, top, 0);
    bindex[2] = _obtainChunkGeo(&ci, left,  bot, 0);
    bindex[3] = _obtainChunkGeo(&ci, right, bot, 0);

    // Second pass to bump any cached chunks (if needed).
    if (bindex[0] < 0)
        _obtainChunkGeo(&ci, left,  top, 1);
    if (bindex[1] < 0)
        _obtainChunkGeo(&ci, right, top, 1);
    if (bindex[2] < 0)
        _obtainChunkGeo(&ci, left,  bot, 1);
    if (bindex[3] < 0)
        _obtainChunkGeo(&ci, right, bot, 1);

    usedMask = ci.geoUsedMask;
    }

    {
    float matrix[16];

    gr->time = ((float) getTicks()) * 0.001;
    memcpy(matrix, unitMatrix, sizeof(matrix));

    if (gr->blockCount) {
        glUseProgram(gr->shadeWorld);
        glUniform2f(gr->worldScroll, gr->tilesVDim, gr->time);
        glActiveTexture(GL_TEXTURE0 + GTU_SHADOW);
        glBindTexture(GL_TEXTURE_2D, gr->shadowTex);
    } else {
        glUseProgram(gr->shadeColor);
        glUniform2f(gr->slocScroll, gr->tilesVDim, gr->time);
    }

    glActiveTexture(GL_TEXTURE0 + GTU_CMAP);
    glBindTexture(GL_TEXTURE_2D, gr->tilesTex);
    glActiveTexture(GL_TEXTURE0 + GTU_MATERIAL);
    glBindTexture(GL_TEXTURE_2D, gr->tilesMat);

    glDisable(GL_BLEND);

    for (i = 0; i < 4; ++i) {
        if (usedMask & (1 << i)) {
            // Position chunk in viewport.
            matrix[ MAT_X ] = (float) (cloc[i].x - cx) * VIEW_TILE_SIZE;
            matrix[ MAT_Y ] = (float) (cy - cloc[i].y) * VIEW_TILE_SIZE;
            glUniformMatrix4fv(gr->slocTrans, 1, GL_FALSE, matrix);

            glBindVertexArray(gr->vao[ GLOB_MAP_CHUNK0 + i ]);
            glDrawArrays(GL_TRIANGLES, 0, gr->mapChunkVertCount);
        }
    }
    }
}

/*
 * Copyright (c) 2018-2021, NVIDIA CORPORATION.  All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2018-2021 NVIDIA CORPORATION
 * SPDX-License-Identifier: Apache-2.0
 */
 //--------------------------------------------------------------------
 
 // Simple class to contain GLSL shaders/programs

#ifndef GLSL_PROGRAM_H
#define GLSL_PROGRAM_H

#include <include_gl.h>
#include <stdio.h>

class GLSLProgram
{
public:
    // construct program from strings
    GLSLProgram(const char*progName=NULL);
	GLSLProgram(const char *vsource, const char *fsource);
    GLSLProgram(const char *vsource, const char *gsource, const char *fsource,
                GLenum gsInput = GL_POINTS, GLenum gsOutput = GL_TRIANGLE_STRIP, int maxVerts=4);

	~GLSLProgram();

	void enable();
	void disable();

	void setUniform1f(const GLchar *name, GLfloat x);
	void setUniform2f(const GLchar *name, GLfloat x, GLfloat y);
    void setUniform2fv(const GLchar *name, float *v) { setUniformfv(name, v, 2, 1); }
    void setUniform3f(const GLchar *name, float x, float y, float z);
    void setUniform3fv(const GLchar *name, float *v) { setUniformfv(name, v, 3, 1); }
    void setUniform4f(const GLchar *name, float x, float y=0.0f, float z=0.0f, float w=0.0f);
	void setUniformfv(const GLchar *name, GLfloat *v, int elementSize, int count=1);
    void setUniformMatrix4fv(const GLchar *name, GLfloat *m, bool transpose);

	void setUniform1i(const GLchar *name, GLint x);
	void setUniform2i(const GLchar *name, GLint x, GLint y);
    void setUniform3i(const GLchar *name, int x, int y, int z);

	void bindTexture(const GLchar *name, GLuint tex, GLenum target, GLint unit);
	void bindImage  (const GLchar *name, GLint unit, GLuint tex, GLint level, GLboolean layered, GLint layer, GLenum access, GLenum format);

	inline GLuint getProgId() { return mProg; }
	
    GLuint compileProgram(const char *vsource, const char *gsource, const char *fsource,
                          GLenum gsInput = GL_POINTS, GLenum gsOutput = GL_TRIANGLE_STRIP, int maxVerts=4);
    GLuint compileProgramFromFiles(const char *vFilename,  const char *gFilename, const char *fFilename,
                       GLenum gsInput = GL_POINTS, GLenum gsOutput = GL_TRIANGLE_STRIP, int maxVerts=4);
    void setShaderNames(const char*ProgName, const char *VSName=NULL,const char *GSName=NULL,const char *FSName=NULL);
    static bool setIncludeFromFile(const char *includeName, const char* filename);
    static void setIncludeFromString(const char *includeName, const char* str);
private:
    static char *readTextFile(const char *filename);
    char *curVSName, *curFSName, *curGSName, *curProgName;

	GLuint mProg;
    static char const* incPaths[];
};

#endif

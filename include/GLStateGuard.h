//
//  GLStateGuard.h
//  MotionPath
//
//  RAII guard for OpenGL state management.
//  Automatically saves and restores OpenGL state on scope exit.
//

#ifndef GLSTATEGUARD_H
#define GLSTATEGUARD_H

// OpenGL headers (cross-platform)
#ifdef __APPLE__
    #include <OpenGL/gl.h>
#else
    #include <GL/gl.h>
#endif

// RAII guard for OpenGL blend state
class GLBlendGuard
{
public:
    GLBlendGuard()
    {
        wasEnabled = glIsEnabled(GL_BLEND);
        glGetIntegerv(GL_BLEND_SRC, &srcFactor);
        glGetIntegerv(GL_BLEND_DST, &dstFactor);
    }

    ~GLBlendGuard()
    {
        if (wasEnabled)
            glEnable(GL_BLEND);
        else
            glDisable(GL_BLEND);
        glBlendFunc(srcFactor, dstFactor);
    }

    // Non-copyable
    GLBlendGuard(const GLBlendGuard&) = delete;
    GLBlendGuard& operator=(const GLBlendGuard&) = delete;

private:
    GLboolean wasEnabled;
    GLint srcFactor;
    GLint dstFactor;
};

// RAII guard for OpenGL line smooth state
class GLLineSmoothGuard
{
public:
    GLLineSmoothGuard()
    {
        wasEnabled = glIsEnabled(GL_LINE_SMOOTH);
        glGetIntegerv(GL_LINE_SMOOTH_HINT, &hintValue);
    }

    ~GLLineSmoothGuard()
    {
        if (wasEnabled)
            glEnable(GL_LINE_SMOOTH);
        else
            glDisable(GL_LINE_SMOOTH);
        glHint(GL_LINE_SMOOTH_HINT, hintValue);
    }

    // Non-copyable
    GLLineSmoothGuard(const GLLineSmoothGuard&) = delete;
    GLLineSmoothGuard& operator=(const GLLineSmoothGuard&) = delete;

private:
    GLboolean wasEnabled;
    GLint hintValue;
};

// RAII guard for OpenGL point smooth state
class GLPointSmoothGuard
{
public:
    GLPointSmoothGuard()
    {
        wasEnabled = glIsEnabled(GL_POINT_SMOOTH);
    }

    ~GLPointSmoothGuard()
    {
        if (wasEnabled)
            glEnable(GL_POINT_SMOOTH);
        else
            glDisable(GL_POINT_SMOOTH);
    }

    // Non-copyable
    GLPointSmoothGuard(const GLPointSmoothGuard&) = delete;
    GLPointSmoothGuard& operator=(const GLPointSmoothGuard&) = delete;

private:
    GLboolean wasEnabled;
};

// Combined RAII guard for common drawing state
// Saves and restores: blend, line smooth, line width, point size
class GLDrawStateGuard
{
public:
    GLDrawStateGuard()
    {
        blendEnabled = glIsEnabled(GL_BLEND);
        lineSmoothEnabled = glIsEnabled(GL_LINE_SMOOTH);
        glGetIntegerv(GL_BLEND_SRC, &blendSrc);
        glGetIntegerv(GL_BLEND_DST, &blendDst);
        glGetFloatv(GL_LINE_WIDTH, &lineWidth);
        glGetFloatv(GL_POINT_SIZE, &pointSize);
    }

    ~GLDrawStateGuard()
    {
        // Restore blend state
        if (blendEnabled)
            glEnable(GL_BLEND);
        else
            glDisable(GL_BLEND);
        glBlendFunc(blendSrc, blendDst);

        // Restore line smooth state
        if (lineSmoothEnabled)
            glEnable(GL_LINE_SMOOTH);
        else
            glDisable(GL_LINE_SMOOTH);

        // Restore line width and point size
        glLineWidth(lineWidth);
        glPointSize(pointSize);
    }

    // Non-copyable
    GLDrawStateGuard(const GLDrawStateGuard&) = delete;
    GLDrawStateGuard& operator=(const GLDrawStateGuard&) = delete;

private:
    GLboolean blendEnabled;
    GLboolean lineSmoothEnabled;
    GLint blendSrc;
    GLint blendDst;
    GLfloat lineWidth;
    GLfloat pointSize;
};

// RAII guard for push/pop attrib pattern
// Replaces manual glPushAttrib/glPopAttrib with scope-based management
class GLAttribGuard
{
public:
    explicit GLAttribGuard(GLbitfield mask)
    {
        glPushAttrib(mask);
    }

    ~GLAttribGuard()
    {
        glPopAttrib();
    }

    // Non-copyable
    GLAttribGuard(const GLAttribGuard&) = delete;
    GLAttribGuard& operator=(const GLAttribGuard&) = delete;
};

// RAII guard for push/pop client attrib
class GLClientAttribGuard
{
public:
    explicit GLClientAttribGuard(GLbitfield mask)
    {
        glPushClientAttrib(mask);
    }

    ~GLClientAttribGuard()
    {
        glPopClientAttrib();
    }

    // Non-copyable
    GLClientAttribGuard(const GLClientAttribGuard&) = delete;
    GLClientAttribGuard& operator=(const GLClientAttribGuard&) = delete;
};

// RAII guard for push/pop matrix
class GLMatrixGuard
{
public:
    explicit GLMatrixGuard(GLenum matrixMode = GL_MODELVIEW)
        : mode(matrixMode)
    {
        glGetIntegerv(GL_MATRIX_MODE, &previousMode);
        glMatrixMode(mode);
        glPushMatrix();
    }

    ~GLMatrixGuard()
    {
        glMatrixMode(mode);
        glPopMatrix();
        glMatrixMode(previousMode);
    }

    // Non-copyable
    GLMatrixGuard(const GLMatrixGuard&) = delete;
    GLMatrixGuard& operator=(const GLMatrixGuard&) = delete;

private:
    GLenum mode;
    GLint previousMode;
};

#endif // GLSTATEGUARD_H

#define GL_GLEXT_PROTOTYPES
#include <Geode/Geode.hpp>
#include <Geode/modify/CCNode.hpp>
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>

using namespace geode::prelude;

namespace Visibility {
    GLuint program = 0;
    GLuint screenTexture = 0;
    GLint screenWidth = 0;
    GLint screenHeight = 0;
    GLint screenUniform = -1;
    GLint gammaUniform = -1;
    GLint brightnessUniform = -1;
    bool failed = false;

    constexpr char const* vertexShader = R"(
        #version 120
        varying vec2 vTexCoord;
        void main() {
            gl_Position = gl_Vertex;
            vTexCoord = gl_MultiTexCoord0.st;
        }
    )";

    constexpr char const* fragmentShader = R"(
        #version 120
        uniform sampler2D uScreen;
        uniform float uGamma;
        uniform float uBrightness;
        varying vec2 vTexCoord;

        void main() {
            vec4 source = texture2D(uScreen, vTexCoord);
            vec3 lifted = pow(max(source.rgb, vec3(0.0)), vec3(1.0 / uGamma));
            lifted *= uBrightness;
            gl_FragColor = vec4(clamp(lifted, 0.0, 1.0), source.a);
        }
    )";

    bool compileShader(GLuint shader, char const* source) {
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);

        GLint success = GL_FALSE;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (success == GL_TRUE) return true;

        GLint length = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        std::string message(static_cast<size_t>(length > 1 ? length : 1), '\0');
        glGetShaderInfoLog(shader, length, nullptr, message.data());
        log::error("Visibility shader compile failed: {}", message);
        return false;
    }

    bool initialize() {
        if (program) return true;
        if (failed) return false;

        GLuint vertex = glCreateShader(GL_VERTEX_SHADER);
        GLuint fragment = glCreateShader(GL_FRAGMENT_SHADER);
        if (!vertex || !fragment ||
            !compileShader(vertex, vertexShader) ||
            !compileShader(fragment, fragmentShader)) {
            if (vertex) glDeleteShader(vertex);
            if (fragment) glDeleteShader(fragment);
            failed = true;
            return false;
        }

        program = glCreateProgram();
        glAttachShader(program, vertex);
        glAttachShader(program, fragment);
        glLinkProgram(program);
        glDeleteShader(vertex);
        glDeleteShader(fragment);

        GLint linked = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &linked);
        if (linked != GL_TRUE) {
            GLint length = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
            std::string message(static_cast<size_t>(length > 1 ? length : 1), '\0');
            glGetProgramInfoLog(program, length, nullptr, message.data());
            log::error("Visibility shader link failed: {}", message);
            glDeleteProgram(program);
            program = 0;
            failed = true;
            return false;
        }

        screenUniform = glGetUniformLocation(program, "uScreen");
        gammaUniform = glGetUniformLocation(program, "uGamma");
        brightnessUniform = glGetUniformLocation(program, "uBrightness");

        glGenTextures(1, &screenTexture);
        glBindTexture(GL_TEXTURE_2D, screenTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        log::info("Gameplay Visibility shader initialized.");
        return true;
    }

    void apply() {
        const double amount = Mod::get()->getSettingValue<double>("visibility");
        if (amount <= 0.001 || !initialize()) return;

        GLint viewport[4] = {0, 0, 0, 0};
        GLint previousProgram = 0;
        GLint previousActiveTexture = GL_TEXTURE0;
        glGetIntegerv(GL_VIEWPORT, viewport);
        glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &previousActiveTexture);

        const GLint width = viewport[2];
        const GLint height = viewport[3];
        if (width <= 0 || height <= 0) return;

        glActiveTexture(GL_TEXTURE0);
        GLint previousTexture = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
        glBindTexture(GL_TEXTURE_2D, screenTexture);

        if (width != screenWidth || height != screenHeight) {
            screenWidth = width;
            screenHeight = height;
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0,
                         GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        }

        // Copy the already-rendered game frame into the texture before drawing
        // the corrected image back over the same viewport.
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                            viewport[0], viewport[1], width, height);

        const GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
        const GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
        const GLboolean scissorWasEnabled = glIsEnabled(GL_SCISSOR_TEST);
        const GLboolean stencilWasEnabled = glIsEnabled(GL_STENCIL_TEST);
        const GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);

        glViewport(viewport[0], viewport[1], width, height);
        glDisable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_STENCIL_TEST);
        glDisable(GL_CULL_FACE);

        const float t = static_cast<float>(std::clamp(amount, 0.0, 1.0));
        const float gamma = 1.0f + 1.25f * t;
        const float brightness = 1.0f + 0.16f * t;

        glUseProgram(program);
        glUniform1i(screenUniform, 0);
        glUniform1f(gammaUniform, gamma);
        glUniform1f(brightnessUniform, brightness);

        glBegin(GL_QUADS);
            glTexCoord2f(0.0f, 0.0f); glVertex2f(-1.0f, -1.0f);
            glTexCoord2f(1.0f, 0.0f); glVertex2f( 1.0f, -1.0f);
            glTexCoord2f(1.0f, 1.0f); glVertex2f( 1.0f,  1.0f);
            glTexCoord2f(0.0f, 1.0f); glVertex2f(-1.0f,  1.0f);
        glEnd();

        glUseProgram(static_cast<GLuint>(previousProgram));
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture));
        glActiveTexture(static_cast<GLenum>(previousActiveTexture));
        glViewport(viewport[0], viewport[1], width, height);

        if (blendWasEnabled) glEnable(GL_BLEND); else glDisable(GL_BLEND);
        if (depthWasEnabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
        if (scissorWasEnabled) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
        if (stencilWasEnabled) glEnable(GL_STENCIL_TEST); else glDisable(GL_STENCIL_TEST);
        if (cullWasEnabled) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    }

    void cleanup() {
        if (screenTexture) glDeleteTextures(1, &screenTexture);
        if (program) glDeleteProgram(program);
        screenTexture = 0;
        program = 0;
        screenWidth = 0;
        screenHeight = 0;
    }
}

class $modify(VisibilityNode, CCNode) {
    void visit() {
        CCNode::visit();

        // The active root scene is visited after its children. Applying the
        // post-process here means the full rendered frame is available.
        if (this == CCScene::get()) {
            Visibility::apply();
        }
    }
};

$on_mod(Unloaded) {
    // Geode unloads mods during shutdown. The GL context may already be gone,
    // so avoid issuing GL deletion calls from this callback.
    Visibility::program = 0;
    Visibility::screenTexture = 0;
}

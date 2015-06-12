/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015
              Vladimír Vondruš <mosra@centrum.cz>

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included
    in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
    THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

#include <Corrade/Utility/Assert.h>
#include <Corrade/Utility/Resource.h>

#include "Magnum/AbstractShaderProgram.h"
#include "Magnum/Buffer.h"
#include "Magnum/Mesh.h"
#include "Magnum/PrimitiveQuery.h"
#include "Magnum/Shader.h"
#include "Magnum/TransformFeedback.h"
#include "Magnum/Math/Vector2.h"
#include "Magnum/Test/AbstractOpenGLTester.h"

namespace Magnum { namespace Test {

struct PrimitiveQueryGLTest: AbstractOpenGLTester {
    explicit PrimitiveQueryGLTest();

    void wrap();

    #ifndef MAGNUM_TARGET_GLES
    void primitivesGenerated();
    #endif
    void transformFeedbackPrimitivesWritten();
};

PrimitiveQueryGLTest::PrimitiveQueryGLTest() {
    addTests({&PrimitiveQueryGLTest::wrap,

              #ifndef MAGNUM_TARGET_GLES
              &PrimitiveQueryGLTest::primitivesGenerated,
              #endif
              &PrimitiveQueryGLTest::transformFeedbackPrimitivesWritten});
}

void PrimitiveQueryGLTest::wrap() {
    #ifndef MAGNUM_TARGET_GLES
    if(!Context::current()->isExtensionSupported<Extensions::GL::ARB::transform_feedback2>())
        CORRADE_SKIP(Extensions::GL::ARB::transform_feedback2::string() + std::string(" is not available."));
    #endif

    GLuint id;
    glGenQueries(1, &id);

    /* Releasing won't delete anything */
    {
        auto query = PrimitiveQuery::wrap(id, PrimitiveQuery::Target::TransformFeedbackPrimitivesWritten, ObjectFlag::DeleteOnDestruction);
        CORRADE_COMPARE(query.release(), id);
    }

    /* ...so we can wrap it again */
    PrimitiveQuery::wrap(id, PrimitiveQuery::Target::TransformFeedbackPrimitivesWritten);
    glDeleteQueries(1, &id);
}

#ifndef MAGNUM_TARGET_GLES
void PrimitiveQueryGLTest::primitivesGenerated() {
    if(!Context::current()->isExtensionSupported<Extensions::GL::EXT::transform_feedback>())
        CORRADE_SKIP(Extensions::GL::EXT::transform_feedback::string() + std::string(" is not available."));

    struct MyShader: AbstractShaderProgram {
        typedef Attribute<0, Vector2> Position;

        explicit MyShader() {
            Shader vert(Version::GL210, Shader::Type::Vertex);

            CORRADE_INTERNAL_ASSERT_OUTPUT(vert.addSource(
                "attribute lowp vec4 position;\n"
                "void main() {\n"
                "    gl_Position = position;\n"
                "}\n").compile());

            attachShader(vert);
            bindAttributeLocation(Position::Location, "position");
            CORRADE_INTERNAL_ASSERT_OUTPUT(link());
        }
    } shader;

    Buffer vertices;
    vertices.setData({nullptr, 9*sizeof(Vector2)}, BufferUsage::StaticDraw);

    Mesh mesh;
    mesh.setPrimitive(MeshPrimitive::Triangles)
        .setCount(9)
        .addVertexBuffer(vertices, 0, MyShader::Position());

    MAGNUM_VERIFY_NO_ERROR();

    PrimitiveQuery q{PrimitiveQuery::Target::PrimitivesGenerated};
    q.begin();

    Renderer::enable(Renderer::Feature::RasterizerDiscard);
    mesh.draw(shader);

    q.end();
    const bool availableBefore = q.resultAvailable();
    const UnsignedInt count = q.result<UnsignedInt>();
    const bool availableAfter = q.resultAvailable();

    MAGNUM_VERIFY_NO_ERROR();
    CORRADE_VERIFY(!availableBefore);
    CORRADE_VERIFY(availableAfter);
    CORRADE_COMPARE(count, 3);
}
#endif

void PrimitiveQueryGLTest::transformFeedbackPrimitivesWritten() {
    #ifndef MAGNUM_TARGET_GLES
    if(!Context::current()->isExtensionSupported<Extensions::GL::ARB::transform_feedback2>())
        CORRADE_SKIP(Extensions::GL::ARB::transform_feedback2::string() + std::string(" is not available."));
    #endif

    struct MyShader: AbstractShaderProgram {
        explicit MyShader() {
            #ifndef MAGNUM_TARGET_GLES
            Shader vert(Version::GL300, Shader::Type::Vertex);
            #else
            Shader vert(Version::GLES300, Shader::Type::Vertex);
            Shader frag(Version::GLES300, Shader::Type::Fragment);
            #endif

            CORRADE_INTERNAL_ASSERT_OUTPUT(vert.addSource(
                "out mediump vec2 outputData;\n"
                "void main() {\n"
                "    outputData = vec2(1.0, -1.0);\n"
                "}\n").compile());
            #ifndef MAGNUM_TARGET_GLES
            attachShader(vert);
            #else
            /* ES for some reason needs both vertex and fragment shader */
            CORRADE_INTERNAL_ASSERT_OUTPUT(frag.addSource("void main() {}\n").compile());
            attachShaders({vert, frag});
            #endif

            setTransformFeedbackOutputs({"outputData"}, TransformFeedbackBufferMode::SeparateAttributes);
            CORRADE_INTERNAL_ASSERT_OUTPUT(link());
        }
    } shader;

    Buffer output;
    output.setData({nullptr, 18*sizeof(Vector2)}, BufferUsage::StaticDraw);

    Mesh mesh;
    mesh.setPrimitive(MeshPrimitive::Triangles)
        .setCount(9);

    MAGNUM_VERIFY_NO_ERROR();

    TransformFeedback feedback;
    feedback.attachBuffer(0, output);

    PrimitiveQuery q{PrimitiveQuery::Target::TransformFeedbackPrimitivesWritten};
    q.begin();

    Renderer::enable(Renderer::Feature::RasterizerDiscard);

    mesh.draw(shader); /* Draw once without XFB (shouldn't be counted) */
    feedback.begin(shader, TransformFeedback::PrimitiveMode::Triangles);
    mesh.draw(shader);
    feedback.end();

    q.end();
    const UnsignedInt count = q.result<UnsignedInt>();

    MAGNUM_VERIFY_NO_ERROR();
    CORRADE_COMPARE(count, 3); /* Three triangles (9 vertices) */
}

}}

MAGNUM_GL_TEST_MAIN(Magnum::Test::PrimitiveQueryGLTest)

/***************************************************************************
 * installable external renderer
 ***************************************************************************/

#include "external_renderer_shader.h"

#include "glm/gtc/matrix_transform.hpp"
#include "objects/material.h"
#include "objects/components/texture_capturer.h"
#include "objects/textures/external_renderer_texture.h"
#include "util/gvr_log.h"

static GVRF_ExternalRenderer externalRenderer = NULL;

void GVRF_installExternalRenderer(GVRF_ExternalRenderer fct) {
    externalRenderer = fct;
}

namespace gvr {

void ExternalRendererShader::render(RenderState* rstate, RenderData* render_data, Material* mtl_unused) {
    if (externalRenderer == NULL) {
        LOGE("External renderer not installed");
        return;
    }

    Material* material = render_data->pass(0)->material();
    if (material == NULL) {
        LOGE("No material");
        return;
    }

    Texture *texture = material->getTexture("main_texture");
    if (texture->getTarget() != ExternalRendererTexture::TARGET) {
        LOGE("External renderer only takes external renderer textures");
        return;
    }

    Mesh* mesh = render_data->mesh();
    if (mesh == NULL) {
        LOGE("No mesh!?");
        return;
    }

    if (mesh->vertices().empty()) {
        LOGE("No vertices!?");
        return;
    }

    {
        const glm::vec3& min_corner = mesh->getBoundingVolume().min_corner();
        scratchBuffer[0] = min_corner[0];
        scratchBuffer[1] = min_corner[1];
        scratchBuffer[2] = min_corner[2];
        const glm::vec3& max_corner = mesh->getBoundingVolume().max_corner();
        scratchBuffer[3] = max_corner[0];
        scratchBuffer[4] = max_corner[1];
        scratchBuffer[5] = max_corner[2];
    }

    //state invariants for surface flinger
    //Oculus leaves buffers bound before calling us; SurfaceFlinger is ES 2.0
    //so the following two lines ensure that SF doesn't end up using incorrect
    //buffers
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glActiveTexture(GL_TEXTURE0);

    TextureCapturer *capturer(render_data->get_texture_capturer());
    if (!capturer || !capturer->getAndClearPendingCapture()) {
        // Original rendering
        externalRenderer(reinterpret_cast<ExternalRendererTexture*>(texture)->getData(),
                         scratchBuffer, 6,
                         glm::value_ptr(rstate->uniforms.u_mvp), 16,
                         glm::value_ptr(*mesh->getVec2Vector("a_texcoord").data()), mesh->getVec2Vector("a_texcoord").size() * 2,
                         material->getFloat("opacity"));
    } else {
        // Capture texture in RenderTexture
        capturer->beginCapture();

        const std::vector<glm::vec3>& vertices(mesh->vertices());
        if (!vertices.empty()) {
            float halfWidth = fabs(vertices[0][0]);
            float halfHeight = fabs(vertices[0][1]);

            glm::mat4 mvp = capturer->getMvpMatrix(halfWidth, halfHeight);
            externalRenderer(reinterpret_cast<ExternalRendererTexture*>(texture)->getData(),
                    scratchBuffer, 6,
                    glm::value_ptr(mvp), 16,
                    glm::value_ptr(*mesh->getVec2Vector("a_texcoord").data()), mesh->getVec2Vector("a_texcoord").size() * 2,
                    1.0);
        }

        capturer->startReadBack();
        capturer->endCapture();

        // Render to original target
        capturer->render(rstate, render_data);

        // Callback
        capturer->callback(TCCB_NEW_CAPTURE, 0);
    }

    checkGLError("ExternalRendererShader::render");
}

}


/**
 * Copyright (c) 2017 Melown Technologies SE
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * *  Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "map.hpp"

namespace vts
{

DrawTask::DrawTask() :
    color{0,0,0,1}, uvClip{-1,-1,2,2}, center{0,0,0},
    externalUv(false), flatShading(false)
{
    for (int i = 0; i < 16; i++)
        mv[i] = i % 4 == i / 4 ? 1 : 0;
    for (int i = 0; i < 9; i++)
        uvm[i] = i % 3 == i / 3 ? 1 : 0;
}

DrawTask::DrawTask(const RenderTask &r, const MapImpl *m) :
    mesh(r.mesh ? r.mesh->info.userData : nullptr),
    uvClip{0,0,1,1},
    externalUv(r.externalUv),
    flatShading(r.flatShading || m->options.debugFlatShading)
{
    assert(r.ready());
    if (r.textureColor)
        texColor = r.textureColor->info.userData;
    if (r.textureMask)
        texMask = r.textureMask->info.userData;
    for (int i = 0; i < 4; i++)
        this->color[i] = r.color[i];
    mat4f mv = (m->renderer.viewRender * r.model).cast<float>();
    for (int i = 0; i < 16; i++)
        this->mv[i] = mv(i);
    for (int i = 0; i < 9; i++)
        this->uvm[i] = r.uvm(i);
    vec3f c = vec4to3(r.model * vec4(0,0,0,1)).cast<float>();
    vecToRaw(c, center);
}

DrawTask::DrawTask(const RenderTask &r, const float *uvClip, const MapImpl *m)
 : DrawTask(r, m)
{
    for (int i = 0; i < 4; i++)
        this->uvClip[i] = uvClip[i];

    // debug
    /*
    if (uvClip[0] != -1)
    {
        for (int i = 0; i < 3; i++)
            color[i] *= 0.4;
    }
    */
}

MapDraws::MapDraws()
{
    memset(&camera, 0, sizeof(camera));
}

void MapDraws::clear()
{
    opaque.clear();
    transparent.clear();
    Infographic.clear();
}

void MapDraws::sortOpaqueFrontToBack()
{
    vec3 e = rawToVec3(camera.eye);
    std::sort(opaque.begin(), opaque.end(), [e](const DrawTask &a,
              const DrawTask &b) {
        vec3 va = rawToVec3(a.center).cast<double>() - e;
        vec3 vb = rawToVec3(b.center).cast<double>() - e;
        return dot(va, va) < dot(vb, vb);
    });
}

RenderTask::RenderTask() : model(identityMatrix4()),
    uvm(identityMatrix3().cast<float>()),
    color(1,1,1,1), externalUv(false), flatShading(false)
{}

bool RenderTask::ready() const
{
    if (mesh && !*mesh)
        return false;
    if (textureColor && !*textureColor)
        return false;
    if (textureMask && !*textureMask)
        return false;
    return true;
}

} // namespace vts


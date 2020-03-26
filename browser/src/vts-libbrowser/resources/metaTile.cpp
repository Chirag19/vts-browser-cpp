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

#include "../metaTile.hpp"
#include "../fetchTask.hpp"
#include "../mapConfig.hpp"
//#include "../tilesetMapping.hpp"

#include <dbglog/dbglog.hpp>

namespace vts
{

MetaTile::MetaTile(vts::MapImpl *map, const std::string &name) :
    Resource(map, name),
    vtslibs::vts::MetaTile(vtslibs::vts::TileId(), 0)
{}

void MetaTile::decode()
{
    detail::BufferStream w(fetch->reply.content);
    *(vtslibs::vts::MetaTile*)this
            = vtslibs::vts::loadMetaTile(w, 5, name);
    vtslibs::vts::MetaTile::for_each([](vtslibs::vts::TileId,
        vtslibs::vts::MetaNode &node) {
            // override display size to 1024
            node.displaySize = 1024;
        });
    info.ramMemoryCost += sizeof(*this);
    uint32 side = 1 << 5;
    info.ramMemoryCost += side * side * sizeof(vtslibs::vts::MetaNode);
}

FetchTask::ResourceType MetaTile::resourceType() const
{
    return FetchTask::ResourceType::MetaTile;
}

} // namespace vts

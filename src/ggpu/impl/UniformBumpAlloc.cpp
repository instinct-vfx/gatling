//
// Copyright (C) 2024 Pablo Delgado Krämer
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "UniformBumpAlloc.h"

#include "DelayedResourceDestroyer.h"
#include "SyncBuffer.h"

#include <assert.h>

// TODO: we need a VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC descriptor pool
// also: VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC descriptor binding
namespace gtl
{
  GgpuUniformBumpAlloc::GgpuUniformBumpAlloc(CgpuDevice device,
                                             GgpuStager& stager,
                                             GgpuDelayedResourceDestroyer& delayedResourceDestroyer,
                                             uint32_t bufferSize)
    : m_device(device)
    , m_stager(stager)
    , m_delayedResourceDestroyer(delayedResourceDestroyer)
    , m_bufferSize(bufferSize)
  {
    CgpuPhysicalDeviceProperties props;
    bool result = cgpuGetPhysicalDeviceProperties(m_device, &props);
    assert(result);
    assert(bufferSize < props.maxUniformBufferRange);

    m_minUniformBufferOffsetAlignment = props.minUniformBufferOffsetAlignment;
  }

  GgpuUniformBumpAlloc::~GgpuUniformBumpAlloc()
  {
  }

  uint32_t GgpuUniformBumpAlloc::alloc(size_t size, const uint8_t* data)
  {
    if (!m_buffer)
    {
      m_buffer = std::make_unique<GgpuSyncBuffer>(
        m_device,
        m_stager,
        m_delayedResourceDestroyer,
        0,// rem: elementSize,
        GgpuSyncBuffer::UpdateStrategy::PreferPersistentMapping,
        CGPU_BUFFER_USAGE_FLAG_UNIFORM_BUFFER
      );

      if (!m_buffer->resize(size))
      {
        assert(false);
        return UINT32_MAX;
      }
    }

    uint32_t a = m_minUniformBufferOffsetAlignment;
    uint32_t cursorEnd = (m_cursor + size + a - 1) / a * a;
    uint32_t cursorStart = (cursorEnd >= m_bufferSize) ? 0 : m_cursor;

    // TODO: copy to SyncBuffer

    m_cursor = cursorStart;
    return cursorStart;
  }
}

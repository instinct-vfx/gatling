//
// Copyright (C) 2019-2022 Pablo Delgado Krämer
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

#include "stager.h"

#include <assert.h>
#include <algorithm>

const static uint64_t BUFFER_SIZE = 64 * 1024 * 1024;

namespace gtl
{
  GiStager::GiStager(CgpuDevice device)
    : m_device(device)
  {
  }

  GiStager::~GiStager()
  {
    assert(m_stagedBytes == 0);
  }

  bool GiStager::allocate()
  {
    m_stagingBuffer = { CGPU_INVALID_HANDLE };
    m_commandBuffer = { CGPU_INVALID_HANDLE };
    m_fence = { CGPU_INVALID_HANDLE };

    // Try to use ReBAR if available.
    bool bufferCreated = cgpuCreateBuffer(m_device,
                                          CGPU_BUFFER_USAGE_FLAG_TRANSFER_SRC,
                                          CGPU_MEMORY_PROPERTY_FLAG_DEVICE_LOCAL | CGPU_MEMORY_PROPERTY_FLAG_HOST_VISIBLE,
                                          BUFFER_SIZE,
                                          &m_stagingBuffer);

    if (!bufferCreated)
    {
      if (!cgpuCreateBuffer(m_device,
                            CGPU_BUFFER_USAGE_FLAG_TRANSFER_SRC,
                            CGPU_MEMORY_PROPERTY_FLAG_HOST_VISIBLE | CGPU_MEMORY_PROPERTY_FLAG_HOST_CACHED,
                            BUFFER_SIZE,
                            &m_stagingBuffer))
      {
        goto fail;
      }
    }

    if (!cgpuCreateCommandBuffer(m_device, &m_commandBuffer))
      goto fail;

    if (!cgpuCreateFence(m_device, &m_fence))
      goto fail;

    if (!cgpuMapBuffer(m_device, m_stagingBuffer, (void**) &m_mappedMem))
      goto fail;

    if (!cgpuBeginCommandBuffer(m_commandBuffer))
      goto fail;

    return true;

fail:
    free();
    return false;
  }

  void GiStager::free()
  {
    assert(m_stagedBytes == 0);
    cgpuEndCommandBuffer(m_commandBuffer);
    if (m_mappedMem)
    {
      cgpuUnmapBuffer(m_device, m_stagingBuffer);
    }
    cgpuDestroyFence(m_device, m_fence);
    cgpuDestroyCommandBuffer(m_device, m_commandBuffer);
    cgpuDestroyBuffer(m_device, m_stagingBuffer);
  }

  bool GiStager::flush()
  {
    if (m_stagedBytes == 0)
    {
      return true;
    }

    if (!cgpuFlushMappedMemory(m_device, m_stagingBuffer, 0, m_stagedBytes))
      return false;

    if (!cgpuResetFence(m_device, m_fence))
      return false;

    if (!cgpuEndCommandBuffer(m_commandBuffer))
      return false;

    if (!cgpuSubmitCommandBuffer(m_device, m_commandBuffer, m_fence))
      return false;

    if (!cgpuWaitForFence(m_device, m_fence))
      return false;

    if (!cgpuBeginCommandBuffer(m_commandBuffer))
      return false;

    m_stagedBytes = 0;
    return true;
  }

  bool GiStager::stageToBuffer(const uint8_t* src, uint64_t size, CgpuBuffer dst, uint64_t dstBaseOffset)
  {
    auto copyFunc = [this, dst, dstBaseOffset](uint64_t srcOffset, uint64_t dstOffset, uint64_t size) {
      return cgpuCmdCopyBuffer(
        m_commandBuffer,
        m_stagingBuffer,
        srcOffset,
        dst,
        dstBaseOffset + dstOffset,
        size
      );
    };

    return stage(src, size, copyFunc);
  }

  bool GiStager::stageToImage(const uint8_t* src, uint64_t size, CgpuImage dst)
  {
    if (size > BUFFER_SIZE)
    {
      return false;
    }

    uint64_t availableSpace = BUFFER_SIZE - m_stagedBytes;

    if (availableSpace < size)
    {
      // We don't partially copy to an image, so we need to make sure everything fits into the staging buffer.
      if (!flush())
      {
        return false;
      }
    }

    auto copyFunc = [this, dst](uint64_t srcOffset, uint64_t dstOffset, uint64_t size) {
      return cgpuCmdCopyBufferToImage(
        m_commandBuffer,
        m_stagingBuffer,
        srcOffset,
        dst
      );
    };

    return stage(src, size, copyFunc);
  }

  bool GiStager::stage(const uint8_t* src, uint64_t size, CopyFunc copyFunc)
  {
    uint64_t bytesToStage = size;

    while (bytesToStage > 0)
    {
      uint64_t bytesAlreadyCopied = size - bytesToStage;

      uint64_t availableSpace = BUFFER_SIZE - m_stagedBytes;
      uint64_t memcpyByteCount = std::min(bytesToStage, availableSpace);
      bytesToStage -= memcpyByteCount;

      memcpy(&m_mappedMem[m_stagedBytes], &src[bytesAlreadyCopied], memcpyByteCount);

      if (!copyFunc(m_stagedBytes, bytesAlreadyCopied, memcpyByteCount))
      {
        return false;
      }

      m_stagedBytes += memcpyByteCount;

      if (m_stagedBytes != BUFFER_SIZE)
      {
        continue;
      }

      if (flush())
      {
        continue;
      }

      return false;
    }

    return true;
  }
}

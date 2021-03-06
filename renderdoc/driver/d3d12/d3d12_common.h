/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Baldur Karlsson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#pragma once

#define INITGUID

#include "api/replay/renderdoc_replay.h"
#include "core/core.h"
#include "driver/dx/official/d3d12.h"
#include "driver/dx/official/dxgi1_4.h"
#include "driver/shaders/dxbc/dxbc_compile.h"
#include "serialise/serialiser.h"

// similar to RDCUNIMPLEMENTED but for things that are hit often so we don't want to fire the
// debugbreak.
#define D3D12NOTIMP(...) RDCDEBUG("D3D12 not implemented - " __VA_ARGS__)

UINT GetNumSubresources(const D3D12_RESOURCE_DESC *desc);

class WrappedID3D12Device;

template <typename RealType>
class RefCounter12
{
private:
  unsigned int m_iRefcount;
  bool m_SelfDeleting;

protected:
  RealType *m_pReal;

  void SetSelfDeleting(bool selfDelete) { m_SelfDeleting = selfDelete; }
  // used for derived classes that need to soft ref but are handling their
  // own self-deletion

public:
  RefCounter12(RealType *real, bool selfDelete = true)
      : m_pReal(real), m_iRefcount(1), m_SelfDeleting(selfDelete)
  {
  }
  virtual ~RefCounter12() {}
  unsigned int GetRefCount() { return m_iRefcount; }
  //////////////////////////////
  // implement IUnknown
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject)
  {
    return RefCountDXGIObject::WrapQueryInterface(m_pReal, riid, ppvObject);
  }

  ULONG STDMETHODCALLTYPE AddRef()
  {
    InterlockedIncrement(&m_iRefcount);
    return m_iRefcount;
  }
  ULONG STDMETHODCALLTYPE Release()
  {
    unsigned int ret = InterlockedDecrement(&m_iRefcount);
    if(ret == 0 && m_SelfDeleting)
      delete this;
    return ret;
  }

  unsigned int SoftRef(WrappedID3D12Device *device)
  {
    unsigned int ret = AddRef();
    if(device)
      device->SoftRef();
    else
      RDCWARN("No device pointer, is a deleted resource being AddRef()d?");
    return ret;
  }

  unsigned int SoftRelease(WrappedID3D12Device *device)
  {
    unsigned int ret = Release();
    if(device)
      device->SoftRelease();
    else
      RDCWARN("No device pointer, is a deleted resource being Release()d?");
    return ret;
  }
};

#define IMPLEMENT_IUNKNOWN_WITH_REFCOUNTER_CUSTOMQUERY                \
  ULONG STDMETHODCALLTYPE AddRef() { return RefCounter12::AddRef(); } \
  ULONG STDMETHODCALLTYPE Release() { return RefCounter12::Release(); }
#define IMPLEMENT_FUNCTION_SERIALISED(ret, func) \
  ret func;                                      \
  bool CONCAT(Serialise_, func);

template <>
void Serialiser::Serialise(const char *name, D3D12_RESOURCE_DESC &el);
template <>
void Serialiser::Serialise(const char *name, D3D12_COMMAND_QUEUE_DESC &el);
template <>
void Serialiser::Serialise(const char *name, D3D12_SHADER_BYTECODE &el);
template <>
void Serialiser::Serialise(const char *name, D3D12_GRAPHICS_PIPELINE_STATE_DESC &el);
template <>
void Serialiser::Serialise(const char *name, D3D12_COMPUTE_PIPELINE_STATE_DESC &el);
template <>
void Serialiser::Serialise(const char *name, D3D12_INDEX_BUFFER_VIEW &el);
template <>
void Serialiser::Serialise(const char *name, D3D12_VERTEX_BUFFER_VIEW &el);
template <>
void Serialiser::Serialise(const char *name, D3D12_RESOURCE_BARRIER &el);
template <>
void Serialiser::Serialise(const char *name, D3D12_HEAP_PROPERTIES &el);
template <>
void Serialiser::Serialise(const char *name, D3D12_DESCRIPTOR_HEAP_DESC &el);
template <>
void Serialiser::Serialise(const char *name, D3D12_SAMPLER_DESC &el);
template <>
void Serialiser::Serialise(const char *name, D3D12_CONSTANT_BUFFER_VIEW_DESC &el);
template <>
void Serialiser::Serialise(const char *name, D3D12_SHADER_RESOURCE_VIEW_DESC &el);
template <>
void Serialiser::Serialise(const char *name, D3D12_RENDER_TARGET_VIEW_DESC &el);
template <>
void Serialiser::Serialise(const char *name, D3D12_DEPTH_STENCIL_VIEW_DESC &el);
template <>
void Serialiser::Serialise(const char *name, D3D12_UNORDERED_ACCESS_VIEW_DESC &el);
template <>
void Serialiser::Serialise(const char *name, D3D12_CLEAR_VALUE &el);

struct D3D12Descriptor;
template <>
void Serialiser::Serialise(const char *name, D3D12Descriptor &el);

#pragma region Chunks

#define D3D12_CHUNKS                                                                         \
  D3D12_CHUNK_MACRO(DEVICE_INIT = FIRST_CHUNK_ID, "ID3D12Device::Initialisation")            \
  D3D12_CHUNK_MACRO(SET_RESOURCE_NAME, "ID3D12Object::SetName")                              \
  D3D12_CHUNK_MACRO(RELEASE_RESOURCE, "IUnknown::Release")                                   \
  D3D12_CHUNK_MACRO(CREATE_SWAP_BUFFER, "IDXGISwapChain::GetBuffer")                         \
                                                                                             \
  D3D12_CHUNK_MACRO(CAPTURE_SCOPE, "Capture")                                                \
                                                                                             \
  D3D12_CHUNK_MACRO(PUSH_EVENT, "BeginEvent")                                                \
  D3D12_CHUNK_MACRO(SET_MARKER, "SetMarker")                                                 \
  D3D12_CHUNK_MACRO(POP_EVENT, "EndEvent")                                                   \
                                                                                             \
  D3D12_CHUNK_MACRO(DEBUG_MESSAGES, "DebugMessageList")                                      \
                                                                                             \
  D3D12_CHUNK_MACRO(CONTEXT_CAPTURE_HEADER, "ContextBegin")                                  \
  D3D12_CHUNK_MACRO(CONTEXT_CAPTURE_FOOTER, "ContextEnd")                                    \
                                                                                             \
  D3D12_CHUNK_MACRO(SET_SHADER_DEBUG_PATH, "SetShaderDebugPath")                             \
                                                                                             \
  D3D12_CHUNK_MACRO(CREATE_COMMAND_QUEUE, "ID3D12Device::CreateCommandQueue")                \
  D3D12_CHUNK_MACRO(CREATE_COMMAND_ALLOCATOR, "ID3D12Device::CreateCommandAllocator")        \
  D3D12_CHUNK_MACRO(CREATE_COMMAND_LIST, "ID3D12Device::CreateCommandList")                  \
                                                                                             \
  D3D12_CHUNK_MACRO(CREATE_GRAPHICS_PIPE, "ID3D12Device::CreateGraphicsPipeline")            \
  D3D12_CHUNK_MACRO(CREATE_COMPUTE_PIPE, "ID3D12Device::CreateComputePipeline")              \
  D3D12_CHUNK_MACRO(CREATE_DESCRIPTOR_HEAP, "ID3D12Device::CreateDescriptorHeap")            \
  D3D12_CHUNK_MACRO(CREATE_ROOT_SIG, "ID3D12Device::CreateRootSignature")                    \
                                                                                             \
  D3D12_CHUNK_MACRO(CREATE_COMMITTED_RESOURCE, "ID3D12Device::CreateCommittedResource")      \
                                                                                             \
  D3D12_CHUNK_MACRO(CREATE_FENCE, "ID3D12Device::CreateFence")                               \
                                                                                             \
  D3D12_CHUNK_MACRO(CLOSE_LIST, "ID3D12GraphicsCommandList::Close")                          \
  D3D12_CHUNK_MACRO(RESET_LIST, "ID3D12GraphicsCommandList::Reset")                          \
                                                                                             \
  D3D12_CHUNK_MACRO(RESOURCE_BARRIER, "ID3D12GraphicsCommandList::ResourceBarrier")          \
                                                                                             \
  D3D12_CHUNK_MACRO(DRAW_INDEXED_INST, "ID3D12GraphicsCommandList::DrawIndexedInstanced")    \
  D3D12_CHUNK_MACRO(COPY_BUFFER, "ID3D12GraphicsCommandList::CopyBufferRegion")              \
                                                                                             \
  D3D12_CHUNK_MACRO(CLEAR_RTV, "ID3D12GraphicsCommandList::ClearRenderTargetView")           \
                                                                                             \
  D3D12_CHUNK_MACRO(SET_TOPOLOGY, "ID3D12GraphicsCommandList::IASetPrimitiveTopology")       \
  D3D12_CHUNK_MACRO(SET_IBUFFER, "ID3D12GraphicsCommandList::IASetIndexBuffer")              \
  D3D12_CHUNK_MACRO(SET_VBUFFERS, "ID3D12GraphicsCommandList::IASetVertexBuffers")           \
  D3D12_CHUNK_MACRO(SET_VIEWPORTS, "ID3D12GraphicsCommandList::RSSetViewports")              \
  D3D12_CHUNK_MACRO(SET_SCISSORS, "ID3D12GraphicsCommandList::RSSetScissors")                \
  D3D12_CHUNK_MACRO(SET_PIPE, "ID3D12GraphicsCommandList::SetPipelineState")                 \
  D3D12_CHUNK_MACRO(SET_RTVS, "ID3D12GraphicsCommandList::OMSetRenderTargets")               \
  D3D12_CHUNK_MACRO(SET_GFX_ROOT_SIG, "ID3D12GraphicsCommandList::SetGraphicsRootSignature") \
  D3D12_CHUNK_MACRO(SET_GFX_ROOT_CBV,                                                        \
                    "ID3D12GraphicsCommandList::SetGraphicsRootConstantBufferView")          \
                                                                                             \
  D3D12_CHUNK_MACRO(EXECUTE_CMD_LISTS, "ID3D12GraphicsCommandQueue::ExecuteCommandLists")    \
  D3D12_CHUNK_MACRO(SIGNAL, "ID3D12GraphicsCommandQueue::Signal")                            \
                                                                                             \
  D3D12_CHUNK_MACRO(NUM_D3D12_CHUNKS, "")

enum D3D12ChunkType
{
#undef D3D12_CHUNK_MACRO
#define D3D12_CHUNK_MACRO(enum, string) enum,

  D3D12_CHUNKS
};

#pragma endregion Chunks

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

#include "RenderDelegate.h"
#include "RenderParam.h"
#include "RenderPass.h"
#include "Camera.h"
#include "Mesh.h"
#include "Instancer.h"
#include "RenderBuffer.h"
#include "Material.h"
#include "Tokens.h"
#include "Light.h"

#include <pxr/imaging/hd/resourceRegistry.h>
#include <pxr/base/gf/vec4f.h>

#include <memory>

PXR_NAMESPACE_OPEN_SCOPE

HdGatlingRenderDelegate::HdGatlingRenderDelegate(const HdRenderSettingsMap& settingsMap,
                                                 const MaterialNetworkTranslator& translator)
  : m_translator(translator)
  , m_resourceRegistry(std::make_shared<HdResourceRegistry>())
  , m_renderParam(std::make_unique<HdGatlingRenderParam>())
{
  m_settingDescriptors.push_back(HdRenderSettingDescriptor{ "Samples per pixel", HdGatlingSettingsTokens->spp, VtValue{1} });
  m_settingDescriptors.push_back(HdRenderSettingDescriptor{ "Max bounces", HdGatlingSettingsTokens->max_bounces, VtValue{7} });
  m_settingDescriptors.push_back(HdRenderSettingDescriptor{ "Russian roulette bounce offset", HdGatlingSettingsTokens->rr_bounce_offset, VtValue{3} });
  m_settingDescriptors.push_back(HdRenderSettingDescriptor{ "Russian roulette inverse minimum terminate probability", HdGatlingSettingsTokens->rr_inv_min_term_prob, VtValue{0.95f} });
  m_settingDescriptors.push_back(HdRenderSettingDescriptor{ "Max sample value", HdGatlingSettingsTokens->max_sample_value, VtValue{10.0f} });
  m_settingDescriptors.push_back(HdRenderSettingDescriptor{ "Filter Importance Sampling", HdGatlingSettingsTokens->filter_importance_sampling, VtValue{true} });
  m_settingDescriptors.push_back(HdRenderSettingDescriptor{ "Depth of field", HdGatlingSettingsTokens->depth_of_field, VtValue{false} });
  m_settingDescriptors.push_back(HdRenderSettingDescriptor{ "Light intensity multiplier", HdGatlingSettingsTokens->light_intensity_multiplier, VtValue{1.0f} });
  m_settingDescriptors.push_back(HdRenderSettingDescriptor{ "Next event estimation", HdGatlingSettingsTokens->next_event_estimation, VtValue{false} });

  m_debugSettingDescriptors.push_back(HdRenderSettingDescriptor{ "Progressive accumulation", HdGatlingSettingsTokens->progressive_accumulation, VtValue{true} });

#ifndef NDEBUG
  m_settingDescriptors.insert(m_settingDescriptors.end(), m_debugSettingDescriptors.begin(), m_debugSettingDescriptors.end());
#endif
  _PopulateDefaultSettings(m_settingDescriptors);
#ifdef NDEBUG
  _PopulateDefaultSettings(m_debugSettingDescriptors);
#endif

  for (const auto& setting : settingsMap)
  {
    const TfToken& key = setting.first;
    const VtValue& value = setting.second;

    _settingsMap[key] = value;
  }

  m_giScene = giCreateScene();
}

HdGatlingRenderDelegate::~HdGatlingRenderDelegate()
{
  giDestroyScene(m_giScene);
}

HdRenderSettingDescriptorList HdGatlingRenderDelegate::GetRenderSettingDescriptors() const
{
  return m_settingDescriptors;
}

void HdGatlingRenderDelegate::SetRenderSetting(TfToken const& key, VtValue const& value)
{
#ifdef NDEBUG
  // Disallow changing debug render settings in release config.
  for (const HdRenderSettingDescriptor& descriptor : m_debugSettingDescriptors)
  {
    if (key == descriptor.key)
    {
      return;
    }
  }
#endif
  HdRenderDelegate::SetRenderSetting(key, value);
}

HdRenderPassSharedPtr HdGatlingRenderDelegate::CreateRenderPass(HdRenderIndex* index,
                                                                const HdRprimCollection& collection)
{
  return HdRenderPassSharedPtr(new HdGatlingRenderPass(index, collection, _settingsMap, m_translator, m_giScene));
}

HdResourceRegistrySharedPtr HdGatlingRenderDelegate::GetResourceRegistry() const
{
  return m_resourceRegistry;
}

void HdGatlingRenderDelegate::CommitResources(HdChangeTracker* tracker)
{
  TF_UNUSED(tracker);

  // We delay BVH building and GPU uploads to the next render call.
}

HdInstancer* HdGatlingRenderDelegate::CreateInstancer(HdSceneDelegate* delegate, const SdfPath& id)
{
  return new HdGatlingInstancer(delegate, id);
}

void HdGatlingRenderDelegate::DestroyInstancer(HdInstancer* instancer)
{
  delete instancer;
}

HdAovDescriptor HdGatlingRenderDelegate::GetDefaultAovDescriptor(const TfToken& name) const
{
  TF_UNUSED(name);

  HdAovDescriptor aovDescriptor;
  aovDescriptor.format = HdFormatFloat32Vec4;
  aovDescriptor.multiSampled = false;
  aovDescriptor.clearValue = GfVec4f(0.0f, 0.0f, 0.0f, 0.0f);
  return aovDescriptor;
}

HdRenderParam* HdGatlingRenderDelegate::GetRenderParam() const
{
  return m_renderParam.get();
}

const TfTokenVector SUPPORTED_RPRIM_TYPES =
{
  HdPrimTypeTokens->mesh
};

const TfTokenVector& HdGatlingRenderDelegate::GetSupportedRprimTypes() const
{
  return SUPPORTED_RPRIM_TYPES;
}

HdRprim* HdGatlingRenderDelegate::CreateRprim(const TfToken& typeId, const SdfPath& rprimId)
{
  if (typeId == HdPrimTypeTokens->mesh)
  {
    return new HdGatlingMesh(rprimId);
  }
  else TF_WARN("can't instantiate %s %s\n", typeId.GetText(), rprimId.GetText());

  return nullptr;
}

void HdGatlingRenderDelegate::DestroyRprim(HdRprim* rprim)
{
  delete rprim;
}

const TfTokenVector SUPPORTED_SPRIM_TYPES =
{
  HdPrimTypeTokens->camera,
  HdPrimTypeTokens->material,
  HdPrimTypeTokens->sphereLight,
  HdPrimTypeTokens->distantLight,
  HdPrimTypeTokens->rectLight,
  HdPrimTypeTokens->domeLight,
  HdPrimTypeTokens->simpleLight // Required for usdview domeLight creation
};

const TfTokenVector& HdGatlingRenderDelegate::GetSupportedSprimTypes() const
{
  return SUPPORTED_SPRIM_TYPES;
}

HdSprim* HdGatlingRenderDelegate::CreateSprim(const TfToken& typeId, const SdfPath& sprimId)
{
  if (typeId == HdPrimTypeTokens->camera)
  {
    return new HdGatlingCamera(sprimId);
  }
  else if (typeId == HdPrimTypeTokens->material)
  {
    return new HdGatlingMaterial(sprimId);
  }
  else if (typeId == HdPrimTypeTokens->sphereLight)
  {
    return new HdGatlingSphereLight(sprimId, m_giScene);
  }
  else if (typeId == HdPrimTypeTokens->distantLight)
  {
    return new HdGatlingDistantLight(sprimId, m_giScene);
  }
  else if (typeId == HdPrimTypeTokens->rectLight)
  {
    return new HdGatlingRectLight(sprimId, m_giScene);
  }
  else if (typeId == HdPrimTypeTokens->domeLight)
  {
    return new HdGatlingDomeLight(sprimId, m_giScene);
  }
  else if (typeId == HdPrimTypeTokens->simpleLight)
  {
    return new HdGatlingSimpleLight(sprimId, m_giScene);
  }
  else TF_WARN("can't instantiate %s %s\n", typeId.GetText(), sprimId.GetText());

  return nullptr;
}

HdSprim* HdGatlingRenderDelegate::CreateFallbackSprim(const TfToken& typeId)
{
  const SdfPath& sprimId = SdfPath::EmptyPath();

  return CreateSprim(typeId, sprimId);
}

void HdGatlingRenderDelegate::DestroySprim(HdSprim* sprim)
{
  delete sprim;
}

const TfTokenVector SUPPORTED_BPRIM_TYPES =
{
  HdPrimTypeTokens->renderBuffer
};

const TfTokenVector& HdGatlingRenderDelegate::GetSupportedBprimTypes() const
{
  return SUPPORTED_BPRIM_TYPES;
}

HdBprim* HdGatlingRenderDelegate::CreateBprim(const TfToken& typeId, const SdfPath& bprimId)
{
  if (typeId == HdPrimTypeTokens->renderBuffer)
  {
    return new HdGatlingRenderBuffer(bprimId);
  }
  else TF_WARN("can't instantiate %s %s\n", typeId.GetText(), bprimId.GetText());

  return nullptr;
}

HdBprim* HdGatlingRenderDelegate::CreateFallbackBprim(const TfToken& typeId)
{
  const SdfPath& bprimId = SdfPath::EmptyPath();

  return CreateBprim(typeId, bprimId);
}

void HdGatlingRenderDelegate::DestroyBprim(HdBprim* bprim)
{
  delete bprim;
}

TfToken HdGatlingRenderDelegate::GetMaterialBindingPurpose() const
{
  return HdTokens->full;
}

TfTokenVector HdGatlingRenderDelegate::GetMaterialRenderContexts() const
{
  return TfTokenVector{ HdGatlingRenderContexts->mtlx, HdGatlingRenderContexts->mdl };
}

TfTokenVector HdGatlingRenderDelegate::GetShaderSourceTypes() const
{
  return TfTokenVector{ HdGatlingSourceTypes->mtlx, HdGatlingSourceTypes->mdl };
}

PXR_NAMESPACE_CLOSE_SCOPE

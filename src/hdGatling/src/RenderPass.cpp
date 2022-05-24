#include "RenderPass.h"
#include "RenderBuffer.h"
#include "Camera.h"
#include "Mesh.h"
#include "Instancer.h"
#include "Material.h"
#include "Tokens.h"

#include <pxr/imaging/hd/renderPassState.h>
#include <pxr/imaging/hd/renderDelegate.h>
#include <pxr/imaging/hd/rprim.h>
#include <pxr/base/gf/matrix3d.h>

#include <gi.h>

static const char* DEFAULT_MTLX_DOC =
  "<?xml version=\"1.0\"?>"
  "<materialx version=\"1.38\" colorspace=\"lin_rec709\">"
  "  <UsdPreviewSurface name=\"SR_Invalid\" type=\"surfaceshader\">"
  "    <input name=\"diffuseColor\" type=\"color3\" value=\"1.0, 0.0, 1.0\" />"
  "    <input name=\"roughness\" type=\"float\" value=\"1.0\" />"
  "  </UsdPreviewSurface>"
  "  <surfacematerial name=\"invalid\" type=\"material\">"
  "    <input name=\"surfaceshader\" type=\"surfaceshader\" nodename=\"SR_Invalid\" />"
  "  </surfacematerial>"
  "</materialx>";

PXR_NAMESPACE_OPEN_SCOPE

HdGatlingRenderPass::HdGatlingRenderPass(HdRenderIndex* index,
                                         const HdRprimCollection& collection,
                                         const HdRenderSettingsMap& settings)
  : HdRenderPass(index, collection)
  , m_settings(settings)
  , m_isConverged(false)
  , m_lastSceneStateVersion(UINT32_MAX)
  , m_lastRenderSettingsVersion(UINT32_MAX)
  , m_lastBackgroundColor(GfVec4f(0.0f, 0.0f, 0.0f, 0.0f))
  , m_geomCache(nullptr)
  , m_shaderCache(nullptr)
{
  m_defaultMaterial = giCreateMaterialFromMtlx(DEFAULT_MTLX_DOC);
  assert(m_defaultMaterial);
}

HdGatlingRenderPass::~HdGatlingRenderPass()
{
  if (m_geomCache)
  {
    giDestroyGeomCache(m_geomCache);
  }
  if (m_shaderCache)
  {
    giDestroyShaderCache(m_shaderCache);
  }

  giDestroyMaterial(m_defaultMaterial);
}

bool HdGatlingRenderPass::IsConverged() const
{
  return m_isConverged;
}

void HdGatlingRenderPass::_BakeMeshInstance(const HdGatlingMesh* mesh,
                                            GfMatrix4d transform,
                                            uint32_t materialIndex,
                                            std::vector<gi_face>& faces,
                                            std::vector<gi_vertex>& vertices) const
{
  GfMatrix4d normalMatrix = transform.GetInverse().GetTranspose();

  const std::vector<GfVec3f>& meshPoints = mesh->GetPoints();
  const std::vector<GfVec3f>& meshNormals = mesh->GetNormals();
  const std::vector<GfVec3i>& meshFaces = mesh->GetFaces();
  TF_VERIFY(meshPoints.size() == meshNormals.size());

  uint32_t vertexOffset = vertices.size();

  for (size_t j = 0; j < meshFaces.size(); j++)
  {
    const GfVec3i& vertexIndices = meshFaces[j];

    gi_face face;
    face.v_i[0] = vertexOffset + vertexIndices[0];
    face.v_i[1] = vertexOffset + vertexIndices[1];
    face.v_i[2] = vertexOffset + vertexIndices[2];
    face.mat_index = materialIndex;

    faces.push_back(face);
  }

  for (size_t j = 0; j < meshPoints.size(); j++)
  {
    const GfVec3f& point = meshPoints[j];
    const GfVec3f& normal = meshNormals[j];

    GfVec3f newPoint = transform.Transform(point);
    GfVec3f newNormal = normalMatrix.TransformDir(normal);
    newNormal.Normalize();

    gi_vertex vertex;
    vertex.pos[0] = newPoint[0];
    vertex.pos[1] = newPoint[1];
    vertex.pos[2] = newPoint[2];
    vertex.norm[0] = newNormal[0];
    vertex.norm[1] = newNormal[1];
    vertex.norm[2] = newNormal[2];

    vertices.push_back(vertex);
  }
}

void HdGatlingRenderPass::_BakeMeshes(HdRenderIndex* renderIndex,
                                      GfMatrix4d rootTransform,
                                      std::vector<gi_vertex>& vertices,
                                      std::vector<gi_face>& faces,
                                      std::vector<const gi_material*>& materials) const
{
  vertices.clear();
  faces.clear();

  TfHashMap<SdfPath, uint32_t, SdfPath::Hash> materialMapping;
  materialMapping[SdfPath::EmptyPath()] = 0;

  materials.push_back(m_defaultMaterial);

  for (const auto& rprimId : renderIndex->GetRprimIds())
  {
    const HdRprim* rprim = renderIndex->GetRprim(rprimId);

    if (!dynamic_cast<const HdMesh*>(rprim))
    {
      continue;
    }

    const HdGatlingMesh* mesh = dynamic_cast<const HdGatlingMesh*>(rprim);

    VtMatrix4dArray transforms;
    const SdfPath& instancerId = mesh->GetInstancerId();

    if (instancerId.IsEmpty())
    {
      transforms.resize(1);
      transforms[0] = GfMatrix4d(1.0);
    }
    else
    {
      HdInstancer* boxedInstancer = renderIndex->GetInstancer(instancerId);
      HdGatlingInstancer* instancer = dynamic_cast<HdGatlingInstancer*>(boxedInstancer);

      const SdfPath& meshId = mesh->GetId();
      transforms = instancer->ComputeInstanceTransforms(meshId);
    }

    const SdfPath& materialId = mesh->GetMaterialId();
    uint32_t materialIndex = 0;

    if (materialMapping.find(materialId) != materialMapping.end())
    {
      materialIndex = materialMapping[materialId];
    }
    else
    {
      HdSprim* sprim = renderIndex->GetSprim(HdPrimTypeTokens->material, materialId);
      HdGatlingMaterial* material = dynamic_cast<HdGatlingMaterial*>(sprim);

      if (material)
      {
        const gi_material* giMat = material->GetGiMaterial();

        if (giMat)
        {
          materialIndex = materials.size();
          materials.push_back(giMat);
          materialMapping[materialId] = materialIndex;
        }
      }
    }

    const GfMatrix4d& prototypeTransform = mesh->GetPrototypeTransform();

    for (size_t i = 0; i < transforms.size(); i++)
    {
      GfMatrix4d transform = prototypeTransform * transforms[i] * rootTransform;

      _BakeMeshInstance(mesh, transform, materialIndex, faces, vertices);
    }
  }

  printf("#Vertices: %zu\n", vertices.size());
  printf("#Faces: %zu\n", faces.size());
  fflush(stdout);
}

void HdGatlingRenderPass::_ConstructGiCamera(const HdGatlingCamera& camera, gi_camera& giCamera) const
{
  // We transform the scene into camera space at the beginning, so for
  // subsequent camera transforms, we need to 'substract' the initial transform.
  GfMatrix4d absInvViewMatrix = camera.GetTransform();
  GfMatrix4d relViewMatrix = absInvViewMatrix * m_rootMatrix;

  GfVec3d position = relViewMatrix.Transform(GfVec3d(0.0, 0.0, 0.0));
  GfVec3d forward = relViewMatrix.TransformDir(GfVec3d(0.0, 0.0, -1.0));
  GfVec3d up = relViewMatrix.TransformDir(GfVec3d(0.0, 1.0, 0.0));

  forward.Normalize();
  up.Normalize();

  giCamera.position[0] = (float) position[0];
  giCamera.position[1] = (float) position[1];
  giCamera.position[2] = (float) position[2];
  giCamera.forward[0] = (float) forward[0];
  giCamera.forward[1] = (float) forward[1];
  giCamera.forward[2] = (float) forward[2];
  giCamera.up[0] = (float) up[0];
  giCamera.up[1] = (float) up[1];
  giCamera.up[2] = (float) up[2];
  giCamera.vfov = camera.GetVFov();
}

const HdRenderPassAovBinding* _FilterAovBinding(const HdRenderPassAovBindingVector& aovBindings)
{
  for (const HdRenderPassAovBinding& aovBinding : aovBindings)
  {
    if (aovBinding.aovName != HdAovTokens->color
        && aovBinding.aovName != HdAovTokens->normal
#ifndef NDEBUG
        && aovBinding.aovName != HdGatlingAovTokens->debug_nee
        && aovBinding.aovName != HdGatlingAovTokens->debug_bvh_steps
        && aovBinding.aovName != HdGatlingAovTokens->debug_tri_tests
#endif
       )
    {
      HdGatlingRenderBuffer* renderBuffer = dynamic_cast<HdGatlingRenderBuffer*>(aovBinding.renderBuffer);
      renderBuffer->SetConverged(true);
      continue;
    }

    return &aovBinding;
  }

  return nullptr;
}

gi_aov_id _GetAovId(const TfToken& aovName)
{
  gi_aov_id id = GI_AOV_ID_COLOR;

  if (aovName == HdAovTokens->normal) {
    id = GI_AOV_ID_NORMAL;
  }
  else if (aovName == HdGatlingAovTokens->debug_nee) {
    id = GI_AOV_ID_DEBUG_NEE;
  }
  else if (aovName == HdGatlingAovTokens->debug_bvh_steps) {
    id = GI_AOV_ID_DEBUG_BVH_STEPS;
  }
  else if (aovName == HdGatlingAovTokens->debug_tri_tests) {
    id = GI_AOV_ID_DEBUG_TRI_TESTS;
  }
  else if (aovName != HdAovTokens->color) {
    TF_CODING_ERROR("Invalid AOV id");
  }

  return id;
}

void HdGatlingRenderPass::_Execute(const HdRenderPassStateSharedPtr& renderPassState,
                                   const TfTokenVector& renderTags)
{
  TF_UNUSED(renderTags);

  m_isConverged = false;

  const auto* camera = dynamic_cast<const HdGatlingCamera*>(renderPassState->GetCamera());

  if (!camera)
  {
    return;
  }

  const HdRenderPassAovBindingVector& aovBindings = renderPassState->GetAovBindings();
  if (aovBindings.empty())
  {
    return;
  }

  const HdRenderPassAovBinding* aovBinding = _FilterAovBinding(aovBindings);
  if (!aovBinding)
  {
    TF_RUNTIME_ERROR("AOV not supported");
    return;
  }

  HdGatlingRenderBuffer* renderBuffer = dynamic_cast<HdGatlingRenderBuffer*>(aovBinding->renderBuffer);
  if (renderBuffer->GetFormat() != HdFormatFloat32Vec4)
  {
    TF_RUNTIME_ERROR("Unsupported render buffer format");
    return;
  }

  HdRenderIndex* renderIndex = GetRenderIndex();
  HdChangeTracker& changeTracker = renderIndex->GetChangeTracker();
  HdRenderDelegate* renderDelegate = renderIndex->GetRenderDelegate();

  GfVec4f backgroundColor(0.0f, 0.0f, 0.0f, 0.0f);
  if (aovBinding->clearValue.IsHolding<GfVec4f>())
  {
    backgroundColor = aovBinding->clearValue.Get<GfVec4f>();
  }

  uint32_t sceneStateVersion = changeTracker.GetSceneStateVersion();
  uint32_t renderSettingsStateVersion = renderDelegate->GetRenderSettingsVersion();
  gi_aov_id aovId = _GetAovId(aovBinding->aovName);

  bool sceneChanged = (sceneStateVersion != m_lastSceneStateVersion);
  bool renderSettingsChanged = (renderSettingsStateVersion != m_lastRenderSettingsVersion);
  bool backgroundColorChanged = (backgroundColor != m_lastBackgroundColor);
  bool aovChanged = (aovId != m_lastAovId);

  if (!sceneChanged && !renderSettingsChanged && !backgroundColorChanged)
  {
    renderBuffer->SetConverged(true);
    return;
  }

  renderBuffer->SetConverged(false);

  m_lastSceneStateVersion = sceneStateVersion;
  m_lastRenderSettingsVersion = renderSettingsStateVersion;
  m_lastBackgroundColor = backgroundColor;
  m_lastAovId = aovId;

  bool rebuildGeomCache = !m_geomCache;
#ifndef NDEBUG
  // BVH tri threshold could have been changed - see comment below.
  rebuildGeomCache |= renderSettingsChanged;
#endif

  if (rebuildGeomCache)
  {
    if (m_geomCache)
    {
      giDestroyGeomCache(m_geomCache);
    }

    const SdfPath& cameraId = camera->GetId();
    printf("Building geom cache for camera %s\n", cameraId.GetText());
    fflush(stdout);

    std::vector<gi_vertex> vertices;
    std::vector<gi_face> faces;
    std::vector<const gi_material*> materials;

    // Transform scene into camera space to increase floating point precision.
    GfMatrix4d viewMatrix = camera->GetTransform().GetInverse();

    _BakeMeshes(renderIndex, viewMatrix, vertices, faces, materials);

    gi_geom_cache_params geomParams;
    geomParams.bvh_tri_threshold = m_settings.find(HdGatlingSettingsTokens->bvh_tri_threshold)->second.Get<int>();
    geomParams.next_event_estimation = m_settings.find(HdGatlingSettingsTokens->next_event_estimation)->second.Get<bool>();
    geomParams.face_count = faces.size();
    geomParams.faces = faces.data();
    geomParams.material_count = materials.size();
    geomParams.materials = materials.data();
    geomParams.vertex_count = vertices.size();
    geomParams.vertices = vertices.data();

    m_geomCache = giCreateGeomCache(&geomParams);
    TF_VERIFY(m_geomCache, "Unable to create geom cache");

    m_rootMatrix = viewMatrix;
  }

  bool rebuildShaderCache = !m_shaderCache || aovChanged;
#ifndef NDEBUG
  // HACK: the render settings that require shader recompilation are currently only enabled in non-release builds.
  // After the transition to wavefront, and parallel shader compilation, most of them should be backed by preprocessor
  // defines instead of push constants. Recompilation would then be always required.
  rebuildShaderCache |= renderSettingsChanged;
#endif

  if (m_geomCache && rebuildShaderCache)
  {
    if (m_shaderCache)
    {
      giDestroyShaderCache(m_shaderCache);
    }

    printf("Building shader cache...\n");
    fflush(stdout);

    gi_shader_cache_params shaderParams;
    shaderParams.aov_id = aovId;
    shaderParams.geom_cache = m_geomCache;
    shaderParams.triangle_postponing = m_settings.find(HdGatlingSettingsTokens->triangle_postponing)->second.Get<bool>();

    m_shaderCache = giCreateShaderCache(&shaderParams);
    TF_VERIFY(m_shaderCache, "Unable to create shader cache");
  }

  if (!m_geomCache || !m_shaderCache)
  {
    return;
  }

  gi_camera giCamera;
  _ConstructGiCamera(*camera, giCamera);

  gi_render_params renderParams;
  renderParams.camera = &giCamera;
  renderParams.geom_cache = m_geomCache;
  renderParams.shader_cache = m_shaderCache;
  renderParams.image_width = renderBuffer->GetWidth();
  renderParams.image_height = renderBuffer->GetHeight();
  renderParams.max_bounces = m_settings.find(HdGatlingSettingsTokens->max_bounces)->second.Get<int>();
  renderParams.spp = m_settings.find(HdGatlingSettingsTokens->spp)->second.Get<int>();
  renderParams.rr_bounce_offset = m_settings.find(HdGatlingSettingsTokens->rr_bounce_offset)->second.Get<int>();
  // Workaround for bug https://github.com/PixarAnimationStudios/USD/issues/913
  VtValue rr_inv_min_term_prob = m_settings.find(HdGatlingSettingsTokens->rr_inv_min_term_prob)->second;
  VtValue max_sample_value = m_settings.find(HdGatlingSettingsTokens->max_sample_value)->second;
  renderParams.rr_inv_min_term_prob = float(rr_inv_min_term_prob.Cast<double>().Get<double>());
  renderParams.max_sample_value = float(max_sample_value.Cast<double>().Get<double>());
  for (uint32_t i = 0; i < 4; i++)
  {
    renderParams.bg_color[i] = backgroundColor[i];
  }

  float* img_data = (float*) renderBuffer->Map();

  int32_t result = giRender(&renderParams,
                            img_data);
  TF_VERIFY(result == GI_OK, "Unable to render scene.");

  renderBuffer->Unmap();
  renderBuffer->SetConverged(true);

  m_isConverged = true;
}

PXR_NAMESPACE_CLOSE_SCOPE

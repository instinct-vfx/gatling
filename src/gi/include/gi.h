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

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <memory>
#include <string>
#include <vector>

enum GiStatus
{
  GI_OK = 0,
  GI_ERROR
};

enum GiAovId
{
  GI_AOV_ID_COLOR              = 0,
  GI_AOV_ID_NORMAL             = 1,
  GI_AOV_ID_DEBUG_NEE          = 2,
  GI_AOV_ID_DEBUG_BARYCENTRICS = 3,
  GI_AOV_ID_DEBUG_TEXCOORDS    = 4,
  GI_AOV_ID_DEBUG_BOUNCES      = 5,
  GI_AOV_ID_DEBUG_CLOCK_CYCLES = 6,
  GI_AOV_ID_DEBUG_OPACITY      = 7,
  GI_AOV_ID_DEBUG_TANGENTS     = 8,
  GI_AOV_ID_DEBUG_BITANGENTS   = 9,
  GI_AOV_ID_DEBUG_THIN_WALLED  = 10
};

struct GiAsset;
struct GiGeomCache;
struct GiMaterial;
struct GiMesh;
struct GiMeshInstance;
struct GiShaderCache;
struct GiScene;
struct GiSphereLight;
struct GiDomeLight;

struct GiCameraDesc
{
  float position[3];
  float forward[3];
  float up[3];
  float vfov;
  float fStop;
  float focusDistance;
  float focalLength;
};

struct GiVertex
{
  float pos[3];
  float u;
  float norm[3];
  float v;
  float tangent[3];
  float bitangentSign;
};

struct GiFace
{
  uint32_t v_i[3];
};

struct GiMeshDesc
{
  uint32_t          faceCount;
  GiFace*           faces;
  const GiMaterial* material;
  uint32_t          vertexCount;
  GiVertex*         vertices;
};

struct GiMeshInstance
{
  const GiMesh* mesh;
  float transform[3][4];
};

struct GiShaderCacheParams
{
  GiAovId            aovId;
  GiDomeLight*       domeLight;
  bool               domeLightCameraVisibility;
  bool               filterImportanceSampling;
  uint32_t           materialCount;
  const GiMaterial** materials;
  bool               nextEventEstimation;
  bool               progressiveAccumulation;
  GiScene*           scene;
};

struct GiGeomCacheParams
{
  uint32_t              meshInstanceCount;
  const GiMeshInstance* meshInstances;
  GiShaderCache*        shaderCache;
};

struct GiRenderParams
{
  const GiCameraDesc*  camera;
  const GiGeomCache*   geomCache;
  const GiShaderCache* shaderCache;
  uint32_t             imageWidth;
  uint32_t             imageHeight;
  uint32_t             maxBounces;
  uint32_t             spp;
  uint32_t             rrBounceOffset;
  float                rrInvMinTermProb;
  float                maxSampleValue;
  float                bgColor[4];
  GiScene*             scene;
};

struct GiInitParams
{
  const char* resourcePath;
  const char* shaderPath;
  const std::vector<std::string>& mdlSearchPaths;
  const std::vector<std::string>& mtlxSearchPaths;
};

class GiAssetReader
{
public:
  virtual GiAsset* open(const char* path) = 0;
  virtual size_t size(const GiAsset* asset) const = 0;
  virtual void* data(const GiAsset* asset) const = 0;
  virtual void close(GiAsset* asset) = 0;
  virtual ~GiAssetReader() = default;
};

GiStatus giInitialize(const GiInitParams* params);
void giTerminate();

void giRegisterAssetReader(GiAssetReader* reader);

GiMaterial* giCreateMaterialFromMtlxStr(const char* str);
GiMaterial* giCreateMaterialFromMtlxDoc(const std::shared_ptr<void/*MaterialX::Document*/> doc);
GiMaterial* giCreateMaterialFromMdlFile(const char* filePath, const char* subIdentifier);
void giDestroyMaterial(GiMaterial* mat);

GiMesh* giCreateMesh(const GiMeshDesc* desc);

GiGeomCache* giCreateGeomCache(const GiGeomCacheParams* params);
void giDestroyGeomCache(GiGeomCache* cache);

GiShaderCache* giCreateShaderCache(const GiShaderCacheParams* params);
void giDestroyShaderCache(GiShaderCache* cache);
bool giShaderCacheNeedsRebuild();

void giInvalidateFramebuffer();

int giRender(const GiRenderParams* params, float* rgbaImg);

GiScene* giCreateScene();
void giDestroyScene(GiScene* scene);

GiSphereLight* giCreateSphereLight(GiScene* scene);
void giDestroySphereLight(GiScene* scene, GiSphereLight* light);
void giSetSphereLightPosition(GiSphereLight* light, float* position);

GiDomeLight* giCreateDomeLight(GiScene* scene, const char* filePath);
void giDestroyDomeLight(GiScene* scene, GiDomeLight* light);
void giSetDomeLightTransform(GiDomeLight* light, float* transform3x3);

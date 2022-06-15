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

#include <pxr/base/tf/staticTokens.h>

PXR_NAMESPACE_OPEN_SCOPE

#define HD_GATLING_SETTINGS_TOKENS                   \
  ((spp, "spp"))                                     \
  ((max_bounces, "max-bounces"))                     \
  ((rr_bounce_offset, "rr-bounce-offset"))           \
  ((rr_inv_min_term_prob, "rr-inv-min-term-prob"))   \
  ((max_sample_value, "max-sample-value"))           \
  ((bvh_tri_threshold, "bvh-tri-threshold"))         \
  ((triangle_postponing, "triangle-postponing"))     \
  ((next_event_estimation, "next-event-estimation"))

// mtlx node identifier is given by UsdMtlx.
#define HD_GATLING_NODE_IDENTIFIER_TOKENS            \
  (mdl)                                              \
  (mtlx)

#define HD_GATLING_SOURCE_TYPE_TOKENS                \
  (mdl)                                              \
  (mtlx)

#define HD_GATLING_DISCOVERY_TYPE_TOKENS             \
  (mdl)                                              \
  (mtlx)

#define HD_GATLING_RENDER_CONTEXT_TOKENS             \
  (mdl)                                              \
  (mtlx)

#define HD_GATLING_NODE_CONTEXT_TOKENS               \
  (mdl)                                              \
  (mtlx)

#define HD_GATLING_NODE_METADATA_TOKENS              \
  (subIdentifier)

#define HD_GATLING_AOV_TOKENS                        \
  ((debug_nee, "debug:nee"))                         \
  ((debug_bvh_steps, "debug:bvh_steps"))             \
  ((debug_tri_tests, "debug:tri_tests"))             \
  ((debug_barycentrics, "debug:barycentrics"))       \
  ((debug_texcoords, "debug:texcoords"))

TF_DECLARE_PUBLIC_TOKENS(HdGatlingSettingsTokens, HD_GATLING_SETTINGS_TOKENS);
TF_DECLARE_PUBLIC_TOKENS(HdGatlingNodeIdentifiers, HD_GATLING_NODE_IDENTIFIER_TOKENS);
TF_DECLARE_PUBLIC_TOKENS(HdGatlingSourceTypes, HD_GATLING_SOURCE_TYPE_TOKENS);
TF_DECLARE_PUBLIC_TOKENS(HdGatlingDiscoveryTypes, HD_GATLING_DISCOVERY_TYPE_TOKENS);
TF_DECLARE_PUBLIC_TOKENS(HdGatlingRenderContexts, HD_GATLING_RENDER_CONTEXT_TOKENS);
TF_DECLARE_PUBLIC_TOKENS(HdGatlingNodeContexts, HD_GATLING_NODE_CONTEXT_TOKENS);
TF_DECLARE_PUBLIC_TOKENS(HdGatlingNodeMetadata, HD_GATLING_NODE_METADATA_TOKENS);
TF_DECLARE_PUBLIC_TOKENS(HdGatlingAovTokens, HD_GATLING_AOV_TOKENS);

PXR_NAMESPACE_CLOSE_SCOPE

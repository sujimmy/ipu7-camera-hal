/*
 * Copyright (C) 2024 Intel Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once
#include <vector>

#if defined(GRC_IPU7X)
#include "Ipu7xGraphResolutionConfiguratorAutogen.h"
#include "Ipu7xStaticGraphAutogen.h"
#include "Ipu7xStaticGraphTypesAutogen.h"
#elif defined(GRC_IPU75XA)
#include "Ipu75xaGraphResolutionConfiguratorAutogen.h"
#include "Ipu75xaStaticGraphAutogen.h"
#include "Ipu75xaStaticGraphTypesAutogen.h"
#elif defined(GRC_IPU8)
#include "Ipu8GraphResolutionConfiguratorAutogen.h"
#include "Ipu8StaticGraphAutogen.h"
#include "Ipu8StaticGraphTypesAutogen.h"
#else
#include "GraphResolutionConfiguratorAutogen.h"
#include "StaticGraphAutogen.h"
#include "StaticGraphTypesAutogen.h"
#endif

// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

// In Visual Studio 2003, MS's C++ standard library generates this warning (unreachable code)
#if _MSC_VER == 1300
#pragma warning( disable : 4702 )
#endif

#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>
#pragma warning( push )
#pragma warning( disable : 4267 ) // Bizarre that MS's standard headers have such warnings...
#include <algorithm>
#pragma warning( pop )

#include <boost/config.hpp>
#include <boost/integer_fwd.hpp>
#include <boost/smart_ptr.hpp>

// Use warning level 3 for lexical cast
#pragma warning( push, 3 )
#pragma warning( disable : 4701 4702 4267 )
#include <boost/lexical_cast.hpp>
#pragma warning( pop )

// We'll include the most common maya API headers as well

// for some reason maya seems to think it needs to define bool for us
#ifndef _BOOL
#define _BOOL
#endif

#include <maya/MAngle.h>
#include <maya/MFloatArray.h>
#include <maya/MFloatMatrix.h>
#include <maya/MGlobal.h>
#include <maya/MIntArray.h>
#include <maya/MMatrix.h>
#include <maya/MPoint.h>
#include <maya/MPxNode.h>
#include <maya/MRenderUtil.h>
#include <maya/MString.h>
#include <maya/MTypes.h>
#include <maya/MVector.h>
#include <maya/MVectorArray.h>

#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

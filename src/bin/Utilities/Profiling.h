#pragma once

// Tracy profiler zones. Configure with -DWHEELER_TRACY=ON to build them in; in
// every other build they expand to nothing, so instrumented code costs nothing
// and the extern/tracy submodule is not needed.
//
// Only the macros used in this codebase are stubbed. Add a stub here before
// using another Tracy macro, or the normal build will not compile.
#ifdef WHEELER_TRACY
#   include <tracy/Tracy.hpp>
#else
#   define ZoneScoped
#   define ZoneScopedN(name)
#   define FrameMark
#endif

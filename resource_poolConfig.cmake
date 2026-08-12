include(CMakeFindDependencyMacro)
find_dependency(Boost 1.69 COMPONENTS thread)

include("${CMAKE_CURRENT_LIST_DIR}/resource_poolTargets.cmake")

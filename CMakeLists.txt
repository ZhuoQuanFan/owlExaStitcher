project(exa-stitch LANGUAGES C CXX CUDA)

cmake_minimum_required(VERSION 3.1.3)

# if(DEFINED PYTHON_GLIBCXX_USE_CXX11_ABI)
#   if(PYTHON_GLIBCXX_USE_CXX11_ABI STREQUAL "True")
#     add_definitions(-D_GLIBCXX_USE_CXX11_ABI=1)
#   else()
#     add_definitions(-D_GLIBCXX_USE_CXX11_ABI=0)
#   endif()
# endif()

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  set(IS_STANDALONE_PROJECT 1)
endif()

option(EXA_STANDALONE_PROJECT "Is this a standalone project?" ON)
if(NOT EXA_STANDALONE_PROJECT)
  set(IS_STANDALONE_PROJECT 0)
endif()

message(STATUS "IS_STANDALONE_PROJECT: ${IS_STANDALONE_PROJECT}")

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/cmake)

set(CMAKE_CUDA_STANDARD 17)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)

set(SET_UP_CONFIGURATIONS_DONE 1)
# No reason to set CMAKE_CONFIGURATION_TYPES if it's not a multiconfig generator
# Also no reason mess with CMAKE_BUILD_TYPE if it's a multiconfig generator.
if(CMAKE_CONFIGURATION_TYPES) # multiconfig generator?
  set(CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo;MinSizeRel" CACHE STRING "" FORCE) 
else()
  if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
  endif()
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY HELPSTRING "Choose the type of build")
  # set the valid options for cmake-gui drop-down list
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug;Release;RelWithDebInfo;MinSizeRel")
endif()

list(APPEND CUDA_NVCC_FLAGS "--extended-lambda")

option(BUILD_SHARED_LIBS "Build all libraries as shared libraries instead of static" ON)

# enable_language(CUDA)
set(owl_dir ${CMAKE_CURRENT_SOURCE_DIR}/submodules/owl)
add_subdirectory(${owl_dir} EXCLUDE_FROM_ALL)
include_directories(${OWL_INCLUDES})

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${owl_dir}/owl/cmake/")
include(embed_ptx)

# ------------------------------------------------------------------
# umesh
# ------------------------------------------------------------------

add_subdirectory(submodules/umesh EXCLUDE_FROM_ALL)

# ------------------------------------------------------------------
# bigmesh (FIXME!)
# ------------------------------------------------------------------

set(mpm_model_library "/home/ampere/stefan/umeshviewer-install/lib/libmpm_model.so")
set(mpm_accel_library "/home/ampere/stefan/umeshviewer-install/lib/libmpm_accel.so")
set(bigmesh_libraries ${mpm_model_library} ${mpm_accel_library})
set(bigmesh_include_dir "/home/ampere/stefan/umeshviewer-install/include")
if(EXISTS ${mpm_model_library} AND EXISTS ${mpm_accel_library} AND EXISTS ${bigmesh_include_dir})
  set(BIGMESH_FOUND TRUE)
else()
  set(BIGMESH_FOUND FALSE)
endif()

if(BIGMESH_FOUND)
  include_directories(${bigmesh_include_dir})
  include_directories("${PROJECT_SOURCE_DIR}/submodules/umesh")
  add_definitions(-DHAVE_BIGMESH=1)
endif()

# ------------------------------------------------------------------
# cuteeOwl
# ------------------------------------------------------------------
if(IS_STANDALONE_PROJECT)
  add_subdirectory(${PROJECT_SOURCE_DIR}/submodules/cuteeOwl)
  include_directories(${PROJECT_SOURCE_DIR}/submodules/cuteeOwl)
  include_directories(${QT_OWL_INCLUDES})
  find_package(Qt5Widgets REQUIRED)
  find_package(OpenGL REQUIRED)
endif()

# ------------------------------------------------------------------
# Visionaray, needed for CPU sampler (e.g., used by kd-tree
# partitioner)
# ------------------------------------------------------------------

set(vsnray_dir ${PROJECT_SOURCE_DIR}/submodules/visionaray)
include_directories(${vsnray_dir}/include)

#--- options ----------------------------------------------
set(EXA_DEFINITIONS)

option(EXA_STITCH_MIRROR_EXAJET "Activate mirroring for exajet in kernels" OFF)
if(EXA_STITCH_MIRROR_EXAJET)
  add_definitions(-DEXA_STITCH_MIRROR_EXAJET=1)
  list(APPEND EXA_DEFINITIONS -DEXA_STITCH_MIRROR_EXAJET=1)
endif()

option(EXASTITCH_CUDA_TEXTURE_TF "Use CUDA texture for transfer function" ON)
if(EXASTITCH_CUDA_TEXTURE_TF)
  add_definitions(-DEXASTITCH_CUDA_TEXTURE_TF=1)
  list(APPEND EXA_DEFINITIONS -DEXASTITCH_CUDA_TEXTURE_TF=1)
endif()

set(EXA_STITCH_EXA_BRICK_SAMPLER_MODE 0 CACHE STRING "ABR-BVH: 0, ext.brick-BVH: 1")
add_definitions(-DEXA_STITCH_EXA_BRICK_SAMPLER_MODE=${EXA_STITCH_EXA_BRICK_SAMPLER_MODE})
list(APPEND EXA_DEFINITIONS -DEXA_STITCH_EXA_BRICK_SAMPLER_MODE=${EXA_STITCH_EXA_BRICK_SAMPLER_MODE})

set(EXA_STITCH_EXA_BRICK_TRAVERSAL_MODE 0 CACHE STRING "ABR+optix: 0, grid+DDA: 1, grid+optix: 2, bricks+kdtree: 3, bricks+optix: 4, ext.brick+optix: 5")
add_definitions(-DEXA_STITCH_EXA_BRICK_TRAVERSAL_MODE=${EXA_STITCH_EXA_BRICK_TRAVERSAL_MODE})
list(APPEND EXA_DEFINITIONS -DEXA_STITCH_EXA_BRICK_TRAVERSAL_MODE=${EXA_STITCH_EXA_BRICK_TRAVERSAL_MODE})

option(EXA_STITCH_SEPARATE_INDEX_BUFFERS_PER_UELEM
       "ExaStitch model/sampler use separate buffers per stitching/uelem type (otherwise we just have buffers for gridlets and general uelems"
       OFF)
if(EXA_STITCH_SEPARATE_INDEX_BUFFERS_PER_UELEM)
  add_definitions(-DEXA_STITCH_SEPARATE_INDEX_BUFFERS_PER_UELEM=${EXA_STITCH_SEPARATE_INDEX_BUFFERS_PER_UELEM})
  list(APPEND EXA_DEFINITIONS -DEXA_STITCH_SEPARATE_INDEX_BUFFERS_PER_UELEM=${EXA_STITCH_SEPARATE_INDEX_BUFFERS_PER_UELEM})
endif()

# ------------------------------------------------------------------
# witcher lib
# ------------------------------------------------------------------
include_directories(${PROJECT_SOURCE_DIR})

# OptiX programs (TODO: why does this fail if we move it further down??!?)

embed_ptx( # TODO: move to samplers
  OUTPUT_TARGET
    embedded_AMRCellSampler
  EMBEDDED_SYMBOL_NAMES
    embedded_AMRCellSampler
  PTX_LINK_LIBRARIES
    owl::owl
  SOURCES
    sampler/programs/AMRCellSampler.cu
)

embed_ptx( # TODO: move to samplers
  OUTPUT_TARGET
    embedded_ExaBrickSampler
  EMBEDDED_SYMBOL_NAMES
    embedded_ExaBrickSampler
  PTX_LINK_LIBRARIES
    owl::owl
  SOURCES
    sampler/programs/ExaBrickSampler.cu
)

embed_ptx( # TODO: move to samplers
  OUTPUT_TARGET
    embedded_ExaStitchSampler
  EMBEDDED_SYMBOL_NAMES
    embedded_ExaStitchSampler
  PTX_LINK_LIBRARIES
    owl::owl
  SOURCES
    sampler/programs/ExaStitchSampler.cu
)

embed_ptx( # TODO: move to samplers
  OUTPUT_TARGET
    embedded_QuickClustersSampler
  EMBEDDED_SYMBOL_NAMES
    embedded_QuickClustersSampler
  PTX_LINK_LIBRARIES
    owl::owl
  SOURCES
    sampler/programs/QuickClustersSampler.cu
)


# ------------------------------------------------------------------
# 
# ------------------------------------------------------------------
add_library(witcher_core
  model/ABRs.cpp
  model/AMRCellModel.cpp
  model/BigMeshModel.cpp
  model/ExaBrickModel.cpp
  model/ExaStitchModel.cpp
  model/Model.cpp
  sampler/AMRCellSampler.cpp
  sampler/BigMeshSampler.cpp
  sampler/ExaBrickSampler.cpp
  sampler/ExaBrickSampler.cu
  sampler/ExaBrickSamplerCPU.cpp
  sampler/ExaStitchSampler.cpp
  sampler/ExaStitchSampler.cu
  sampler/QuickClustersSampler.cpp
  sampler/QuickClustersSampler.cu
  sampler/Sampler.cpp
  Grid.cu
  KDTree.cpp
)
target_compile_options(witcher_core PRIVATE $<$<COMPILE_LANGUAGE:CUDA>:${CUDA_NVCC_FLAGS}>)
target_include_directories(witcher_core PUBLIC 
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
)
target_link_libraries(witcher_core PRIVATE
  embedded_AMRCellSampler 
  embedded_ExaBrickSampler 
  embedded_ExaStitchSampler 
  embedded_QuickClustersSampler
  owl::owl umesh
)
set_target_properties(witcher_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(BIGMESH_FOUND)
  target_link_libraries(witcher_core PRIVATE ${bigmesh_libraries})
endif()

target_compile_definitions(witcher_core PUBLIC ${EXA_DEFINITIONS})

# ------------------------------------------------------------------
# 
# ------------------------------------------------------------------

add_subdirectory(tools/amrMakeDualMesh)

if(NOT IS_STANDALONE_PROJECT)
  return()
endif()

# ------------------------------------------------------------------
# 
# ------------------------------------------------------------------

embed_ptx(
  OUTPUT_TARGET
    embedded_deviceCode
  EMBEDDED_SYMBOL_NAMES
    embedded_deviceCode
  PTX_LINK_LIBRARIES
    owl::owl
  SOURCES
    deviceCode.cu
)

add_library(witcher STATIC
  OWLRenderer.cpp
  TriangleMesh.cpp
)
target_link_libraries(witcher PUBLIC
  embedded_deviceCode
  witcher_core 
  owl::owl umesh
  qtOWL ${OPENGL_LIBRARIES}
)
target_compile_options(witcher PRIVATE 
  $<$<COMPILE_LANGUAGE:CUDA>:${CUDA_NVCC_FLAGS}>
)
target_link_directories(witcher PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/submodules/cuteeOwl/qtOWL)

# ------------------------------------------------------------------
# viewer
# ------------------------------------------------------------------
set(CMAKE_AUTOMOC ON)

add_executable(exaStitchViewer LightInteractor.cpp viewer.cpp)
qt5_use_modules(exaStitchViewer Widgets)
target_link_libraries(exaStitchViewer witcher)
set_target_properties(exaStitchViewer PROPERTIES MACOSX_BUNDLE YES)
# QT_OWL_LINK(exaStitchViewer)



# ------------------------------------------------------------------
# headless viewer
# ------------------------------------------------------------------

add_executable(exaStitchHeadlessViewer LightInteractor.cpp  viewer.cpp headless.cpp)
#link with QT5 only because that's more convenient than ifdef'ing
#all the relevant parts in the cpp
qt5_use_modules(exaStitchHeadlessViewer Core)
target_link_libraries(exaStitchHeadlessViewer witcher)
target_compile_definitions(exaStitchHeadlessViewer PUBLIC HEADLESS)
# QT_OWL_LINK(exaStitchViewer)


# ------------------------------------------------------------------
# tools
# ------------------------------------------------------------------

add_executable(exaBrickKdtreeBuilder tools/kdtreeBuilder.cpp)
target_link_libraries(exaBrickKdtreeBuilder witcher)

add_executable(testDataGenerator tools/makeTestData.cpp)
target_link_libraries(testDataGenerator witcher)
option(EXASTITCH_ANARI_DEVICE "Build the exastitch anari device" OFF)
if(EXASTITCH_ANARI_DEVICE)
   add_subdirectory(anari)
endif()







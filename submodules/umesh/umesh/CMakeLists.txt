# ======================================================================== #
# Copyright 2018-2020 Ingo Wald                                            #
#                                                                          #
# Licensed under the Apache License, Version 2.0 (the "License");          #
# you may not use this file except in compliance with the License.         #
# You may obtain a copy of the License at                                  #
#                                                                          #
#     http://www.apache.org/licenses/LICENSE-2.0                           #
#                                                                          #
# Unless required by applicable law or agreed to in writing, software      #
# distributed under the License is distributed on an "AS IS" BASIS,        #
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. #
# See the License for the specific language governing permissions and      #
# limitations under the License.                                           #
# ======================================================================== #

# ------------------------------------------------------------------
# common settings across entire project
# -----------------------------------------------------------------
add_library(umesh)
target_include_directories(umesh PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/>
)

if (WIN32)
  OPTION(UMESH_USE_TBB "Use TBB to parallelize host-side code?" OFF)
else()
  OPTION(UMESH_USE_TBB "Use TBB to parallelize host-side code?" ON)
endif()

if (UMESH_USE_TBB)

  #
  # first try find_package in CONFIG mode, it will be catched by OneTBB
  # if OneTBB is not avaible, then use the old macro
  #
  find_package(TBB CONFIG)
  if (NOT TBB_FOUND)
    find_package(TBB)
  endif()
  
  if (TBB_FOUND)
    #    include_directories(${TBB_INCLUDE_DIR})
    #    set(OWL_CXX_FLAGS "${OWL_CXX_FLAGS} -DOWL_HAVE_TBB=1")
    message(STATUS "#umesh: found TBB, with TBB_INCLUDE_DIR=${TBB_INCLUDE_DIR}")
    set(UMESH_HAVE_TBB ON)
  else()
    message(STATUS "#umesh: TBB not found; falling back to serial execution of umesh::parallel_for")
    set(UMESH_HAVE_TBB OFF)
  endif()
else()
  set(UMESH_HAVE_TBB OFF)
endif()


if (UMESH_HAVE_TBB)
  target_compile_definitions(umesh PUBLIC -DUMESH_HAVE_TBB=1)
  if (TARGET TBB::tbb)
    target_link_libraries(umesh PUBLIC TBB::tbb)
  else()
    target_link_libraries(umesh PUBLIC tbb)
  endif()
endif()

  # find_package(TBB)
  # if (TBB_FOUND)
  #   set(UMESH_HAVE_TBB ON)

  #   message(STATUS "linking umesh to  ${TBB_LIBRARIES}")
  #   if (TARGET TBB::tbb)
  #     target_link_libraries(umesh PUBLIC TBB::tbb)
  #   else()
  #     target_link_libraries(umesh PUBLIC tbb)
  #   endif()
  #   target_include_directories(umesh PUBLIC
  #     ${PROJECT_SOURCE_DIR}
  #     )
  #   target_compile_definitions(umesh PUBLIC -DUMESH_HAVE_TBB=1)
  # else()
  #   set(UMESH_HAVE_TBB OFF)
  #   message("#umesh.cmake: TBB not found; falling back to serial execution of umesh::parallel_for")
  # endif()
#endif()

# try to find CUDA; if we can't find it we'll simply disable all the
# tools that need it
#if (UMESH_USE_CUDA)
#  find_package(CUDA)
#  if (CUDA_FOUND)
#    CUDA_SELECT_NVCC_ARCH_FLAGS(ARCH_FLAGS Auto)
#    LIST(APPEND CUDA_NVCC_FLAGS ${ARCH_FLAGS})
#    #message("selected cuda arch flags '${ARCH_FLAGS}'")
#  endif()
#endif()


# ------------------------------------------------------------------
# main umesh/umesh library and classes
# ------------------------------------------------------------------
target_sources(umesh PRIVATE
  # ------------------------------------------------------------------
  # the class itself, with whatever is requited to create and
  # manipulate it
  # ------------------------------------------------------------------
  
  UMesh.h
  UMesh.cpp
  check.cpp
  
  RemeshHelper.h
  RemeshHelper.cpp

  TetConn.h
  TetConn.cpp

  FaceConn.h
  FaceConn.cpp
  
  # ------------------------------------------------------------------
  # I/O routines that can directly read into a umesh class
  # ------------------------------------------------------------------
  
  # specially modified fun3d file format that uses 64-bit counters and
  # indices
  io/ugrid64.cpp
  
  # "regular" fun3d files use 32-bit indices, so this is the "regular"
  # fun3d format
  io/ugrid32.cpp

  # fun3d _data_ files with scalar fields/variables
  io/fun3dScalars.cpp
  
  # nateive .umesh format
  io/UMesh.cpp

  # "binary-triangle-mesh" format
  io/btm/BTM.cpp

  # tetrahedralize a general umesh into a tet-mesh
  tetrahedralize.cpp

  # extract outer shell faces (tris and/or quads)
  extractShellFaces.cpp
  
  # run marching cubes/marching tets algorithm on a umesh, produce a
  # new umesh with only triangles
  extractIsoSurface.cpp

  # create a new umesh from _only_ the surface elements (and only
  # those vertices required for that)
  extractSurfaceMesh.cpp
  )

#target_link_libraries(umesh
#  PUBLIC
#  umesh_common
#  )


# ------------------------------------------------------------------
# install target
# ------------------------------------------------------------------

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  DESTINATION include
  COMPONENT headers
  FILES_MATCHING
    PATTERN "*.h"
    PATTERN "*.hh"
    PATTERN "*.hpp"
    PATTERN "*.hxx"
    PATTERN "*.inl"
    PATTERN "*.inc"
)

install(TARGETS umesh
    EXPORT umeshConfig
    RUNTIME DESTINATION bin COMPONENT libraries
    LIBRARY DESTINATION lib COMPONENT libraries
    ARCHIVE DESTINATION lib COMPONENT libraries
)

install(EXPORT umeshConfig
    NAMESPACE umesh::
    DESTINATION lib/cmake/umesh
)





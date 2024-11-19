
find_package(VTK COMPONENTS
  CommonColor
  CommonCore
  CommonDataModel
  IOCore
  IOLegacy #ugrid writer
  REQUIRED
)

add_executable(umeshToVTU
  toVTU.cpp
)

target_link_libraries(umeshToVTU PRIVATE umesh ${VTK_LIBRARIES})

vtk_module_autoinit(
  TARGETS umeshToVTU
  MODULES ${VTK_LIBRARIES}
)

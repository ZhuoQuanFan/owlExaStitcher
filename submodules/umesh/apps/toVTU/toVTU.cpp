#include <vtkDoubleArray.h>
#include <vtkPointData.h>
#include <vtkUnstructuredGrid.h>
#include <vtkUnstructuredGridWriter.h>
#include "umesh/io/ugrid64.h"
#include "umesh/io/UMesh.h"

namespace umesh {

  static bool g_verbose = false;
  static bool g_ascii = false;
  static std::string g_filename;
  static std::string g_outname = "out.vtk";

  UMesh::SP load(const std::string &fileName)
  {
    if (fileName.substr(fileName.size()-6) == ".umesh")
      return UMesh::loadFrom(fileName);
    
    if (fileName.substr(fileName.size()-8) == ".ugrid64")
      return io::UGrid64Loader::load(fileName);

    throw std::runtime_error("could not determine input format"
                             " (only supporting ugrid64 or umesh for now)");
  }

  ///////////////////////////////////////////////////////////////////////////////

  static void printUsage() {
    std::cout << "./umeshToVTU <filename> [{--help|-h}]\n"
              << "   [-o <outname>]\n"
              << "   [{--verbose|-v}]\n"
              << "   [{--ascii|-a}]\n";
  }
  
  static void parseCommandLine(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
      std::string arg = argv[i];
      if (arg == "-v" || arg == "--verbose")
        g_verbose = true;
      else if (arg == "--help" || arg == "-h") {
        printUsage();
        std::exit(0);
      } else if (arg == "-a" || arg == "--ascii")
        g_ascii = true;
      else if (arg == "-o")
        g_outname = std::string(argv[++i]);
      else
        g_filename = std::move(arg);
    }
  }
  
  extern "C" int main(int argc, char *argv[]) {
    parseCommandLine(argc, argv);
    if (g_filename.empty()) {
      printf("ERROR: no UMesh file provided\n");
      std::exit(1);
    }

    if (g_verbose) {
      umesh::verbose = true;
    }

    std::cout << "loading umesh from " << g_filename << std::endl;
    UMesh::SP inMesh = load(g_filename);
    inMesh->print();

    vtkNew<vtkPoints> points;

    vtkDoubleArray *data = vtkDoubleArray::New();
    data->SetName("data");
    data->SetNumberOfTuples(inMesh->perVertex->values.size());
    data->SetNumberOfComponents(1);

    for (size_t i=0;i<inMesh->vertices.size();++i) {
      double x[3] = {inMesh->vertices[i].x,
                     inMesh->vertices[i].y,
                     inMesh->vertices[i].z};
      points->InsertPoint(i, x);
      data->InsertComponent(0, i, inMesh->perVertex->values[i]);
    }
  
    vtkNew<vtkUnstructuredGrid> ugrid;
    ugrid->AllocateExact(inMesh->numVolumeElements(),
                         inMesh->vertices.size());

    // Tets
    for (size_t i=0;i<inMesh->tets.size();++i) {
      vtkIdType pts[8] = {
        inMesh->tets[i][0],
        inMesh->tets[i][1],
        inMesh->tets[i][2],
        inMesh->tets[i][3],
        0,0,0,0
      };
      ugrid->InsertNextCell(VTK_TETRA, 4, pts);
    }

    // Pyrs
    for (size_t i=0;i<inMesh->pyrs.size();++i) {
      vtkIdType pts[8] = {
        inMesh->pyrs[i][0],
        inMesh->pyrs[i][1],
        inMesh->pyrs[i][2],
        inMesh->pyrs[i][3],
        inMesh->pyrs[i][4],
        0,0,0
      };
      ugrid->InsertNextCell(VTK_PYRAMID, 5, pts);
    }

    // Wedges
    for (size_t i=0;i<inMesh->wedges.size();++i) {
      vtkIdType pts[8] = {
        inMesh->wedges[i][0],
        inMesh->wedges[i][1],
        inMesh->wedges[i][2],
        inMesh->wedges[i][3],
        inMesh->wedges[i][4],
        inMesh->wedges[i][5],
        0,0
      };
      ugrid->InsertNextCell(VTK_WEDGE, 6, pts);
    }

    // Hexes
    for (size_t i=0;i<inMesh->hexes.size();++i) {
      vtkIdType pts[8] = {
        inMesh->hexes[i][0],
        inMesh->hexes[i][1],
        inMesh->hexes[i][2],
        inMesh->hexes[i][3],
        inMesh->hexes[i][4],
        inMesh->hexes[i][5],
        inMesh->hexes[i][6],
        inMesh->hexes[i][7]
      };
      ugrid->InsertNextCell(VTK_HEXAHEDRON, 8, pts);
    }

    ugrid->SetPoints(points);
    //ugrid->GetCellData()->SetScalars(data);
    ugrid->GetPointData()->SetScalars(data);

    vtkNew<vtkUnstructuredGridWriter> xwriter;
    xwriter->SetInputData(ugrid);
    xwriter->SetFileName(g_outname.c_str());
    if (g_ascii)
      xwriter->SetFileTypeToASCII();
    else
      xwriter->SetFileTypeToBinary();
    xwriter->Write();



    return 0;
  }

} // ::umesh

// ======================================================================== //
// Copyright 2018-2020 Ingo Wald                                            //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#include "umesh/io/ugrid32.h"
#include "umesh/io/UMesh.h"

namespace umesh {

  void usage(const std::string error="")
  {
    if (error != "")
      std::cerr << "\nError : " << error  << "\n\n";

    std::cout << "Usage: ./umeshAttachScalars inFile.umesh -s scalars.floats -o outFile.umesh\n\n";
    std::cout << "Reads inFile.umesh (a unstructured mesh) and scalars.float (a set of scalars, one per vertex of the inFile.umesh), attaches the given scalar field to that inFile.umesh (or replaces whatever inFile.umesh may have had), and write out a new outFile.umesh" << std::endl;
    exit(error != "");
  };
  
  extern "C" int main(int ac, char **av)
  {
    std::string inUMeshFileName;
    std::string inScalarsFileName;
    std::string outFileName;
    for (int i=1;i<ac;i++) {
      const std::string arg = av[i];
      if (arg == "-h")
        usage();
      else if (arg == "-o")
        outFileName = av[++i];
      else if (arg == "-s")
        inScalarsFileName = av[++i];
      else if (arg[0] != '-')
        inUMeshFileName = arg;
      else
        usage("unknown cmd-line arg '"+arg+"'");
    }
    
    if (inUMeshFileName == "") usage("no input umesh file specified");
    if (inScalarsFileName == "") usage("no input scalars file specified");
    if (outFileName == "") usage("no input umesh file specified");

    std::cout << "loading umesh from " << inUMeshFileName << std::endl;
    UMesh::SP in = io::loadBinaryUMesh(inUMeshFileName);

    std::cout << "loading scalars from " << inScalarsFileName << std::endl;
    std::vector<float> scalars = io::loadScalars(inScalarsFileName);
    if (scalars.size() != in->vertices.size()) {
      throw std::runtime_error
        ("num scalars found in "+inScalarsFileName
         +" ("+std::to_string(scalars.size())
         +") does not match number of vertices in umesh file "
         +inUMeshFileName
         +" ("+std::to_string(in->vertices.size())+")"
         );
    }

    std::cout << "attaching scalars to umesh ..." << std::endl;
    in->perVertex = std::make_shared<Attribute>();
    in->perVertex->values = scalars;
    in->perVertex->finalize();
    in->finalize();
    std::cout << "saving result to " << outFileName << std::endl;
    in->saveTo(outFileName);
    std::cout << "done writing:\n" << in->toString(false) << std::endl;
  }
  
} // ::umesh

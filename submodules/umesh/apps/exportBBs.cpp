// ======================================================================== //
// Copyright 2024-2024 Ingo Wald                                            //
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

    std::cout << "Usage: ./umeshExportBBs -o out.bb4 inputs.umesh [input2.umesh ...] \n\n";
    exit(error != "");
  };
  
  extern "C" int main(int ac, char **av)
  {
    std::vector<std::string> inFileNames;
    std::string outFileName = "out.bb4";
    for (int i=1;i<ac;i++) {
      const std::string arg = av[i];
      if (arg == "-h")
        usage();
      else if (arg == "-o")
        outFileName = av[++i];
      else if (arg[0] != '-')
        inFileNames.push_back(arg);
      else
        usage("unknown cmd-line arg '"+arg+"'");
    }

    std::ofstream out(outFileName.c_str(),std::ios::binary);
    for (auto inFileName : inFileNames) {
      std::cout << "loading umesh from " << inFileName << std::endl;
      UMesh::SP in = io::loadBinaryUMesh(inFileName);
      std::cout << " -> got mesh:\n" << in->toString(false) << std::endl;
      std::vector<UMesh::PrimRef> prims = in->createAllPrimRefs();
      for (auto prim : prims) {
        box4f bb = in->getBounds4f(prim);
        out.write((char *)&bb,sizeof(bb));
      }
    }
    std::cout << "done. written all bb4's to " << outFileName << std::endl;
    return 0;
  }
  
} // ::umesh

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

/* information on fund3d data format provided by Pat Moran - greatly
   indebted! All loader code rewritten from scratch. */

#include "umesh/RemeshHelper.h"
#include <fstream>

#ifndef PRINT
# define PRINT(var) std::cout << #var << "=" << var << std::endl;
#ifdef __WIN32__
# define PING std::cout << __FILE__ << "::" << __LINE__ << ": " << __FUNCTION__ << std::endl;
#else
# define PING std::cout << __FILE__ << "::" << __LINE__ << ": " << __PRETTY_FUNCTION__ << std::endl;
#endif
#endif

namespace umesh {

  void usage(const std::string &error)
  {
    if (!error.empty())
      std::cout << "Fatal error: " << error << "\n\n";
    
    std::cout << "Usage: ./umeshRawToGrids -d dimsX dimsY dimsZ -f float|uint8"
      " -o outFileName.umesh inFileName.raw" << std::endl;
    
    exit(!error.empty());
  }

  std::string inFileName;
  vec3i dims(0);
  std::string outFileName = "rawToGrids.umesh";
  std::string inputFormat = "float";
  int brickSize = 8;

  inline float toScalar(float f) { return f; }
  inline float toScalar(uint8_t ui) { return ui/255.f; }

  template<typename T>
  void rawToGrids()
  {
    std::ifstream in(inFileName.c_str(),std::ios::binary);
    UMesh::SP mesh = std::make_shared<UMesh>();

    mesh->perVertex = std::make_shared<Attribute>(0);
    
    std::vector<float> scalars(dims.x*(size_t)dims.y*dims.z);
    std::vector<T> inputs(dims.x*(size_t)dims.y*dims.z);
    in.read((char*)inputs.data(),inputs.size()*sizeof(inputs[0]));
    for (int64_t i=0;i<inputs.size();i++)
      scalars[i] = toScalar(inputs[i]);

    for (int iz=0;iz<dims.z-1;iz+=(brickSize-1)) 
      for (int iy=0;iy<dims.y-1;iy+=(brickSize-1)) 
        for (int ix=0;ix<dims.x-1;ix+=(brickSize-1)) {
          UMesh::Grid g;
          int ex = std::min(ix + (brickSize-1),dims.x-1);
          int ey = std::min(iy + (brickSize-1),dims.y-1);
          int ez = std::min(iz + (brickSize-1),dims.z-1);
          g.domain.lower.x = ix;
          g.domain.lower.y = iy;
          g.domain.lower.z = iz;
          g.domain.upper.x = ex;
          g.domain.upper.y = ey;
          g.domain.upper.z = ez;
          g.numCells.x = (ex-ix);
          g.numCells.y = (ey-iy);
          g.numCells.z = (ez-iz);
          g.scalarsOffset = mesh->gridScalars.size();

          range1f r;
          for (int iiz=iz;iiz<=ez;iiz++)
            for (int iiy=iy;iiy<=ey;iiy++)
              for (int iix=ix;iix<=ex;iix++) {
                float scalar = scalars[iix+dims.x*(iiy+(size_t)dims.y*(iiz))];
                mesh->gridScalars.push_back(scalar);
                if (!isnan(scalar))
                  r.extend(scalar);
              }
          g.domain.lower.w = r.lower;
          g.domain.upper.w = r.upper;
          mesh->grids.push_back(g);
        }
    mesh->finalize();
    mesh->saveTo(outFileName);
  }

}

using namespace umesh;

int main(int ac, const char **av)
{
  for (int i=1;i<ac;i++) {
    const std::string arg = av[i];
    if (arg[0] != '-')
      inFileName = arg;
    else if (arg == "-o")
      outFileName = av[++i];
    else if (arg == "-f" || arg == "-if" || arg == "--format")
      inputFormat = av[++i];
    else if (arg == "-d" || arg == "-dims" || arg == "--dims") {
      dims.x = std::stoi(av[++i]);
      dims.y = std::stoi(av[++i]);
      dims.z = std::stoi(av[++i]);
    } else
      usage("unkonwn cmdline argument '"+arg+"'");
  }
  if (inFileName.empty())
    usage("no input file specified");
  if (outFileName.empty())
    usage("no output file specified");
  if (dims.x <= 0)
    usage("no input volume dims specified");
      
  if (inputFormat == "float")
    rawToGrids<float>();
  else if (inputFormat == "uint8")
    rawToGrids<uint8_t>();
  else
    usage("unknown input format '"+inputFormat+"'");
  return 0;
}
  


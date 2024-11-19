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

#include "umesh/io/ugrid32.h"
#include "umesh/io/ugrid64.h"
#include "umesh/io/fun3dScalars.h"
#include "umesh/RemeshHelper.h"

namespace umesh {
  
  /*! time step to merge in - gets ignored if no scalarPath is set;
    otherwise indiceaes which of the times steps to use; -1 means
    'last one in file' */
  int timeStep = -1;

  /*! variable to load in */
  std::string variable = "";

  void usage(const std::string &error)
  {
    if (!error.empty())
      std::cerr << "Error: " << error << std::endl << std::endl;
    std::cout << "Usage: ./fun3DToUmesh <args>" << std::endl;
    std::cout << "w/ args:" << std::endl;
    std::cout << " -ts <timestep>       : time step to use" << std::endl;
    std::cout << " -var <variableName>  : name of variable to use" << std::endl;
    std::cout << " --grid <filename>    : name of file with the ugrid32 mesh" << std::endl;
    std::cout << " --volume-data <path> : path to where the volume files are" << std::endl;
    std::cout << "" << std::endl;
    std::cout << "To print the variables and time step, specify only the volume path" << std::endl;
    std::cout << "" << std::endl;
    std::cout << "Examples: " << std::endl;
    std::cout << "" << std::endl;
    std::cout << " ./fun3DToUmesh --volume-data /path/crmhl-40-37-wmles-mods_volume_data." << std::endl;
    std::cout << "" << std::endl;
    std::cout << "   -> reads the first volume brick from the given path, " << std::endl;
    std::cout << "      and prints all varables and time steps" << std::endl;
    std::cout << "" << std::endl;
    std::cout << " ./fun3DToUmesh                                            \\" << std::endl;
    std::cout << "   --volume-data /path/crmhl-40-37-wmles-mods_volume_data. \\" << std::endl;
    std::cout << "   --grid /path/crmhl-40-37-wmles-mods.lb8.ugrid           \\" << std::endl;
    std::cout << "   -var vort_mag -ts 133900                                \\ " << std::endl;
    std::cout << "   -o /out-path/rajko-vort_mag-133900.umesh" << std::endl;
    std::cout << "" << std::endl;
    std::cout << "   -> extracts given var and time step to a umesh" << std::endl;
      
    exit(error.empty()?0:1);
  }
  
  void fun3DToUMesh(int ac, char **av)
  {
    std::string outFileName;
    std::string gridFileName;
    std::string volumeDataPath;
    std::string variable;
    
    for (int i=1;i<ac;i++) {
      const std::string arg = av[i];
      if (arg == "-o")
        outFileName = av[++i];
      else if (arg == "--grid")
        gridFileName = av[++i];
      else if (arg == "--volume-data")
        volumeDataPath = av[++i];
      else if (arg == "--verbose" || arg == "-v")
        umesh::verbose = 1;
      else if (arg == "-ts" || arg == "--time-step")
        timeStep = atoi(av[++i]);
      else if (arg == "-var" || arg == "--variable")
        variable = av[++i];
      else
        usage("unknown cmdline arg '"+arg+"'");
    }
    if (volumeDataPath.empty())
      usage("no path to volume data specified");

    if (timeStep < 0 || variable == "") {
      std::string firstFileName = volumeDataPath+std::to_string(1);
      std::vector<std::string> variables;
      std::vector<int> timeSteps;
      io::fun3d::getInfo(firstFileName,variables,timeSteps);
      std::cout << "File Info: " << std::endl;
      std::cout << "variables:";
      for (auto var : variables) std::cout << " " << var;
      std::cout << std::endl;
      std::cout << "timeSteps:";
      for (auto var : timeSteps) std::cout << " " << var;
      std::cout << std::endl;
      exit(0);
    }
    if (outFileName.empty())
      usage("no out file name specified");
    if (gridFileName.empty())
      usage("no grid file name specified");

    std::cout << "loading single mesh (ugrid32 format) from " << gridFileName << std::endl;
    UMesh::SP mesh = io::UGrid32Loader::load(gridFileName);
    mesh->perVertex = std::make_shared<Attribute>(mesh->vertices.size());
    mesh->perVertex->name = variable;
    std::cout << "done loading mesh, got " << mesh->toString() << std::endl;
    size_t numVerticesRead = 0;
    for (int rank=1;true;rank++) {
      std::string scalarsFileName = volumeDataPath // + "volume_data."
        +std::to_string(rank);
      std::cout << "reading time step " << timeStep
                << " from " << scalarsFileName << std::endl;

      std::vector<uint64_t> globalVertexIDs;
      std::vector<float> scalars;
      try {
        scalars = io::fun3d::readTimeStep(scalarsFileName,variable,timeStep,
                                  &globalVertexIDs);
      } catch (std::exception e) {
        break;
      }
      
      for (int i=0;i<scalars.size();i++) 
        mesh->setScalar(globalVertexIDs[i],scalars[i]);
      numVerticesRead += scalars.size();
    }
    if (numVerticesRead < mesh->vertices.size()) {
      usage("didn't read as many vertices as we'd expect!? got " + std::to_string(numVerticesRead)
            + " of expected " + std::to_string(mesh->vertices.size()));
    }

    mesh->finalize();
    mesh->saveTo(outFileName);
  }
  
}

int main(int ac, char **av)
{ umesh::fun3DToUMesh(ac,av); return 0; }


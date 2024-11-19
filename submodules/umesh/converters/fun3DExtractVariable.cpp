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
    std::cout << " --umesh <filename>   : name of the (typically per-rank) umesh file for which the variable needs to get extracted" << std::endl;
    std::cout << " --volume-data <path> : path to where the volume files are" << std::endl;
    std::cout << "" << std::endl;
    std::cout << "To print the variables and time step, specify only the volume path" << std::endl;
    std::cout << "" << std::endl;
    std::cout << "This tool eixsts to 'match' the scalar data in a fun3D model" << std::endl;
    std::cout << "to the results of a umesh-tools based data-parallel" << std::endl;
    std::cout << "(re-)partitioning of this same model." << std::endl;
    std::cout << std::endl;
    std::cout << "The inputs to this tool are supposed to be:" << std::endl;
    std::cout << "a) a fun3D data set consisting of multiple per-rank '*_volume_data.<rank>" << std::endl;
    std::cout << "   files (each of which contains multiple variables and time steps)." << std::endl;
    std::cout << "b) one(!) of the per-rank '.umesh' files that was created" << std::endl;
    std::cout << "   using the ./umeshPartitionSpatially or ./umeshPartitionObjectSpace" << std::endl;
    std::cout << "   tools (when re-partitioning said fun3D model into a user-specified" << std::endl;
    std::cout << "   number of ranks)" << std::endl;
    std::cout << std::endl;
    std::cout << "This tool will then, for the specified rank's .umesh file, find" << std::endl;
    std::cout << "all the input model's scalars for the specified variable," << std::endl;
    std::cout << "across all time steps (or for _the_ one time step specified" << std::endl;
    std::cout << "one the command line, if that was the case), and create the " << std::endl;
    std::cout << "following two outputs:" << std::endl;
    std::cout << std::endl;
    std::cout << "a) a '.scalars' file that contains all the extracted time steps" << std::endl;
    std::cout << "   each in the order of vertices used by this umesh" << std::endl;
    std::cout << "b) a new umesh file with the _last_ written time step in that" << std::endl;
    std::cout << "   umesh's scalars array" << std::endl;
      
    exit(error.empty()?0:1);
  }
  
  void fun3DExtractVariable(int ac, char **av)
  {
    std::string outFileName;
    std::string umeshFileName;
    std::string volumeDataPath;
    std::string variable;
    
    for (int i=1;i<ac;i++) {
      const std::string arg = av[i];
      if (arg == "-o")
        outFileName = av[++i];
      else if (arg == "--umesh")
        umeshFileName = av[++i];
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

    std::vector<std::string> variables;
    std::vector<int> timeSteps;
    std::string firstFileName = volumeDataPath+std::to_string(1);
    io::fun3d::getInfo(firstFileName,variables,timeSteps);
    std::cout << "File Info: " << std::endl;
    std::cout << "variables:";
    for (auto var : variables) std::cout << " " << var;
    std::cout << std::endl;
    std::cout << "timeSteps:";
    for (auto var : timeSteps) std::cout << " " << var;
    std::cout << std::endl;
    if (variable == "") {
      exit(0);
    }
    if (timeStep >= 0)
      timeSteps = { timeStep };
    
    if (outFileName.empty())
      usage("no out file name specified");
    if (umeshFileName.empty())
      usage("no umesh file specified");
    
    std::cout << "loading umesh from " << umeshFileName << std::endl;
    UMesh::SP mesh = UMesh::loadFrom(umeshFileName);
    if (mesh->vertexTags.empty())
      throw std::runtime_error("the umesh file specified doesn't have any vertex tags associated ... you sure that's from a partitioned mesh!?");
    
    mesh->perVertex = std::make_shared<Attribute>(mesh->vertices.size());
    mesh->perVertex->name = variable;
    std::cout << "done loading mesh, got " << mesh->toString() << std::endl;
    
    std::map<size_t,size_t> requestedVertices;
    for (size_t i=0;i<mesh->vertexTags.size();i++)
      requestedVertices[mesh->vertexTags[i]] = i;

    range1f totalValueRange;
    std::ofstream scalarsFile(outFileName+"."+variable+".scalars",std::ios::binary);
    for (int timeStep : timeSteps) {
      std::cout << "----------- extracting time step " << timeStep << " -----------" << std::endl;
      for (int rank=1;true;rank++) {
        std::cout << "[" << rank << "]" << std::flush;
        std::string scalarsFileName = volumeDataPath // + "volume_data."
          +std::to_string(rank);
        // std::cout << "reading time step " << timeStep
        //           << " from " << scalarsFileName << std::endl;
        
        std::vector<uint64_t> globalVertexIDs;
        std::vector<float> scalars;
        try {
          scalars = io::fun3d::readTimeStep(scalarsFileName,variable,timeStep,
                                            &globalVertexIDs);
        } catch (std::exception e) {
          break;
        }
        for (int i=0;i<scalars.size();i++) {
          size_t vertexID = globalVertexIDs[i];
          if (requestedVertices.find(vertexID) == requestedVertices.end())
            // this partitioned umesh doesn't need this vertex...
            continue;
          size_t ourID = requestedVertices[vertexID];
          mesh->setScalar(ourID,scalars[i]);
        }
      }
      std::cout << std::endl;
      mesh->perVertex->finalize();
      totalValueRange.extend(mesh->perVertex->valueRange);
      scalarsFile.write((char *)mesh->perVertex->values.data(),
                        mesh->perVertex->values.size()*sizeof(float));
    }
    
    mesh->finalize();
    mesh->saveTo(outFileName+"."+variable+".umesh");
  }
  
}

int main(int ac, char **av)
{ umesh::fun3DExtractVariable(ac,av); return 0; }


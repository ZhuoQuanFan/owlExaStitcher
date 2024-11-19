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

/* computes an *object* space partitioing of a mesh into bricks (up
   until either max numbre of bricks is reahed, or bricks' sizes fall
   below user-specified max size). stores resulting bricks in one file
   per brick, plus one file containing the bounding box for each
   brick. brick bounds may overlap, but no prim should ever go into
   more than one brick */

#include "umesh/io/ugrid32.h"
#include "umesh/io/UMesh.h"
#include "umesh/RemeshHelper.h"
#include <queue>

namespace umesh {

  bool equalWeight = true;
  
  void usage(const std::string &error = "")
  {
    if (error != "")
      std::cout << "Fatal error: " << error << std::endl << std::endl;
    std::cout << "./umeshPartitionObjectSpace <in.umesh> <args>" << std::endl;
    std::cout << "w/ Args: " << std::endl;
    std::cout << "-o <baseName>\n\tbase path for all output files (there will be multiple)" << std::endl;
    std::cout << "-n|-num-bricks <N>\n\tnumber of bricks to create" << std::endl;
    std::cout << "--max-bricks <N>\n\tmax number of bricks to create, for given -lt" << std::endl;
    std::cout << "-lt|--leaf-threshold <N>\n\tnum prims at which we make a leaf" << std::endl;
    std::cout << std::endl;
    std::cout << "generated files are:" << std::endl;
    std::cout << "<baseName>.bricks : one box3f for each generated brick" << std::endl;
    std::cout << "<baseName>_%05d.umesh : the extracted umeshes for each brick" << std::endl;
    exit( error != "");
  }

  struct Brick {
    std::vector<UMesh::PrimRef> prims;
    box3f bounds;
    box3f centBounds;
  };

  void split(UMesh::SP mesh,
             Brick *in,
             Brick *out[2])
  {
    // PING;
    // PRINT(in->prims.size());
    // PRINT(in->bounds);
    // PRINT(in->centBounds);
    
    if (in->centBounds.lower == in->centBounds.upper)
      throw std::runtime_error("can't split this any more ...");
    std::cout << "splitting brick\tw/ bounds " << in->bounds << " cent " << in->centBounds << std::endl;
    
#if 1
    float bestWeight = 1e30f;
    int bestDim = -1;
    float bestPos = 0.f;
    for (int dim=0;dim<3;dim++) {
      enum { numPlanes = 15 };
      for (int plane=0;plane<numPlanes;plane++) {
        float f = (plane+1.f)/(numPlanes+1.f);
        float pos
          = (1.f-f)*in->centBounds.lower[dim]
          +      f *in->centBounds.upper[dim];
        std::cout << "checking at " << char('x'+dim) << "=" << pos << std::endl;

        std::mutex mutex;
        size_t count[2] = { 0,0 };
        // out[0] = new Brick;
        // out[1] = new Brick;
        parallel_for_blocked
          (0,in->prims.size(),16*1024,[&](size_t begin, size_t end){
            int l_count[2] = { 0,0 };
            for (int i=begin;i<end;i++) {
              auto prim = in->prims[i];
              const box3f pb = mesh->getBounds(prim);
              int side = (pb.center()[dim] < pos) ? 0 : 1;
              l_count[side]++;
            }
            std::lock_guard<std::mutex> lock(mutex);
            count[0] += l_count[0];
            count[1] += l_count[1];
          });
        // for (auto prim : in->prims) {
        //   const box3f pb = mesh->getBounds(prim);
        //   int side = (pb.center()[dim] < pos) ? 0 : 1;
        //   // out[side]->prims.push_back(prim);
        //   // out[side]->bounds.extend(pb);
        //   // out[side]->centBounds.extend(pb.center());
        // }
        float s0 = (float)count[0];//out[0]->prims.size();
        float s1 = (float)count[1];//out[1]->prims.size();
        float s = fabsf(s0-s1)/(s0+s1);
        float sx = in->bounds.size().x;
        float sy = in->bounds.size().y;
        float sz = in->bounds.size().z;
        float weight
          = (.1f+s)
          * max(max(sx,sy),sz)
          / std::max(1e-10f,in->bounds.size()[dim]);
        std::cout << " -> split has " << prettyNumber(count[0])
                  << " vs " << prettyNumber(count[1])
                  << ", weight " << weight << std::endl;
        if (count[0] && count[1] && 
            weight < bestWeight) {
          bestWeight = weight;
          bestPos = pos;
          bestDim = dim;
        }
      }
    }
    int dim = bestDim;
    float pos = bestPos;
    std::cout << "chosen split " << char('x'+dim) << "=" << pos << std::endl;
#else      
    int dim = arg_max(in->centBounds.size());
    float pos = in->centBounds.center()[dim];
#endif
    std::cout << "splitting at " << char('x'+dim) << "=" << pos << std::endl;

    out[0] = new Brick;
    out[1] = new Brick;
    for (auto prim : in->prims) {
      const box3f pb = mesh->getBounds(prim);
      int side = (pb.center()[dim] < pos) ? 0 : 1;
      out[side]->prims.push_back(prim);
      out[side]->bounds.extend(pb);
      out[side]->centBounds.extend(pb.center());
    }
    std::cout << "done splitting " << prettyNumber(in->prims.size()) << " prims\tw/ bounds " << in->bounds << std::endl;
    std::cout << "into L = " << prettyNumber(out[0]->prims.size()) << " prims\tw/ bounds " << out[0]->bounds << std::endl;
    std::cout << " and R = " << prettyNumber(out[1]->prims.size()) << " prims\tw/ bounds " << out[1]->bounds << std::endl;
  }
  
void createInitialBrick(std::priority_queue<std::pair<int,Brick *>> &bricks,
                          UMesh::SP in)
  {
    Brick *brick = new Brick;
    brick->prims = in->createAllPrimRefs();
#if 1
    parallel_for_blocked
      (0,brick->prims.size(),1024,
       [&](size_t begin, size_t end) {
        for (int i=begin;i<end;i++) {
          auto prim = brick->prims[i];
          const box3f pb = in->getBounds(prim);
          brick->bounds.extend(pb);
          brick->centBounds.extend(pb.center());
        }
      });
#else
    for (auto prim : brick->prims) {
      const box3f pb = in->getBounds(prim);
      brick->bounds.extend(pb);
      brick->centBounds.extend(pb.center());
    }
#endif
           
    bricks.push({(int)brick->prims.size(),brick});
  }

  void writeBrick(UMesh::SP in,
                  const std::string &fileBase,
                  Brick *brick)
  {
    std::cout << "creating output brick over " << prettyNumber(brick->prims.size()) << " prims" << std::endl;
    UMesh::SP out = std::make_shared<UMesh>();
    RemeshHelper indexer(*out);
    for (auto prim : brick->prims) 
      indexer.add(in,prim);
    std:: cout << "done reindexing, finalizing umesh" << std::endl;
    out->finalize();
    const std::string fileName = fileBase+".umesh";
    std::cout << "saving out " << fileName
              << " w/ " << prettyNumber(out->size()) << " prims" << std::endl;
    io::saveBinaryUMesh(fileName,out);
    std::cout << "done saving" << std::endl;
  }
  
  extern "C" int main(int ac, char **av)
  {
    std::string inFileName;
    std::string outFileBase;
    int leafThreshold = 1<<30;
    int maxBricks = 1<<30;
    
    for (int i=1;i<ac;i++) {
      const std::string arg = av[i];
      if (arg == "-o")
        outFileBase = av[++i];
      else if (arg == "-lt" || arg == "--leaf-threshold")
        leafThreshold = atoi(av[++i]);
      else if (arg == "-mb" || arg == "--max-bricks")
        maxBricks = atoi(av[++i]);
      else if (arg == "-n" || arg == "--num-bricks") {
        maxBricks = atoi(av[++i]);
        leafThreshold = 1;
      } else if (arg[0] != '-')
        inFileName = arg;
      else
        usage("unknown arg "+arg);
    }
    
    if (outFileBase == "") usage("no output file name specified");
    if (inFileName == "") usage("no input file name specified");
    if (leafThreshold == 1<<30 && maxBricks == 1<<30)
      usage("neither leaf threshold nor max bricks specified");
    std::cout << "loading umesh from " << inFileName << std::endl;
    UMesh::SP in = io::loadBinaryUMesh(inFileName);
    std::cout << "done loading, found " << in->toString() << std::endl;

    std::priority_queue<std::pair<int,Brick *>> bricks;
    createInitialBrick(bricks,in);
    
    while (bricks.size() < maxBricks) {
      auto biggest = bricks.top(); 
      std::cout << "########### currently having " << bricks.size()
                << " bricks, biggest of which has "
                << prettyNumber(biggest.first) << " prims" << std::endl;
      if (biggest.first < leafThreshold)
        break;
      bricks.pop();

      std::cout << "splitting..." << std::endl;
      Brick *half[2];
      split(in,biggest.second,half);
      bricks.push({(int)half[0]->prims.size(),half[0]});
      bricks.push({(int)half[1]->prims.size(),half[1]});
      delete biggest.second;
    }

    std::cout << "done splitting, creating and emitting bricks" << std::endl;
    char ext[20];
    // std::vector<box3f> brickBounds;
    for (int brickID=0;!bricks.empty();brickID++) {
      Brick *brick = bricks.top().second;
      bricks.pop();
      sprintf(ext,"_%05d",brickID);
      writeBrick(in,outFileBase+ext,brick);
      // brickBounds.push_back(brick->bounds);
      delete brick;
    }

    // std::ofstream boundsFile(outFileBase+".bounds",std::ios::binary);
    // io::writeVector(boundsFile,brickBounds);
    // std::cout << "done wirting bounds... done all" << std::endl;
  }
  
} // ::umesh

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

// ours:
#include "UMesh.h"
#include "io/UMesh.h"
#include "io/IO.h"
#include <sstream>


#ifndef PRINT
#ifdef __CUDA_ARCH__
# define PRINT(va) /**/
# define PING /**/
#else
# define PRINT(var) std::cout << #var << "=" << var << std::endl;
#ifdef __WIN32__
# define PING std::cout << __FILE__ << "::" << __LINE__ << ": " << __FUNCTION__ << std::endl;
#else
# define PING std::cout << __FILE__ << "::" << __LINE__ << ": " << __PRETTY_FUNCTION__ << std::endl;
#endif
#endif
#endif

namespace umesh {
  
  const size_t bum_magic = 0x234235568ULL;
  
  /*! can be used to turn on/off logging/diagnostic messages in entire
    umesh library */
  bool verbose = false;

  /*! helper functoin for printf debugging - puts the four elemnt
    sizes into a human-readable (short) string*/
  std::string sizeString(UMesh::SP mesh)
  {
    std::stringstream str;
    str << "v:" << prettyNumber(mesh->vertices.size())
        << ",t:" << prettyNumber(mesh->tets.size())
        << ",p:" << prettyNumber(mesh->pyrs.size())
        << ",w:" << prettyNumber(mesh->wedges.size())
        << ",h:" << prettyNumber(mesh->hexes.size());
    return str.str();
  }
  

  /*! write - binary - to given (bianry) stream */
  void UMesh::writeTo(std::ostream &out) const
  {
    io::writeElement(out,bum_magic);
    io::writeVector(out,vertices);

    // iw - changed that to write 'numPerVertex' rather than always
    // write one - this allows for later switching to more than one
    // attribute
    std::vector<Attribute::SP> attrs = attributes;
    if (perVertex && attributes.empty())
      attrs.push_back(perVertex);

    
    size_t numPerVertexAttributes = attrs.size();
    io::writeElement(out,numPerVertexAttributes);
    for (int i=0;i<numPerVertexAttributes;i++) {
      io::writeString(out,attrs[i]->name);
      io::writeVector(out,attrs[i]->values);
    } 
    size_t numPerElementAttributes = 0;
    io::writeElement(out,numPerElementAttributes);
    
    io::writeVector(out,triangles);
    io::writeVector(out,quads);
    io::writeVector(out,tets);
    io::writeVector(out,pyrs);
    io::writeVector(out,wedges);
    io::writeVector(out,hexes);
    io::writeVector(out,grids);
    io::writeVector(out,gridScalars);
    io::writeVector(out,vertexTags);
  }
  
  /*! write - binary - to given file */
  void UMesh::saveTo(const std::string &fileName) const
  {
    if (size() > 0 && bounds.empty()) {
      throw std::runtime_error("invalid mesh bounds value when saving umesh - did you forget some finalize() somewhere?");
    }

    std::ofstream out(fileName, std::ios_base::binary);
    writeTo(out);
  }

  /*! reads format encoded by version tag '0x234235566ULL' - magic has
      already been read */
  void read_566(UMesh *mesh, std::istream &in)
  {
    io::readVector(in,mesh->vertices,"vertices");
    size_t numPerVertexAttributes = 1;
    if (numPerVertexAttributes) {
      Attribute::SP attr = std::make_shared<Attribute>();
      // io::readString(in,attr->name);
      // PRINT(attr->name);
      io::readVector(in,attr->values,"scalars");
      attr->finalize();
      mesh->attributes.push_back(attr);
    }
    if (!mesh->attributes.empty())
      mesh->perVertex = mesh->attributes[0];
      
    size_t numPerElementAttributes = 0;
    // io::readElement(in,numPerElementAttributes);
    assert(numPerElementAttributes == 0);
    
    io::readVector(in,mesh->triangles,"triangles");
    io::readVector(in,mesh->quads,"quads");
    io::readVector(in,mesh->tets,"tets");
    io::readVector(in,mesh->pyrs,"pyramids");
    io::readVector(in,mesh->wedges,"wedges");
    io::readVector(in,mesh->hexes,"hexes");
    // try {
    if (!in.eof())
      try {
        io::readVector(in,mesh->vertexTags,"vertexTags");
      } catch (...) {
        /* ignore ... */
      }
    mesh->finalize();
  }
  
  /*! read from given (binary) stream */
  void UMesh::readFrom(std::istream &in)
  {
    const size_t bum_magic_old = 0x234235567ULL;
    bool hasGrids = true;
    size_t magic;
    io::readElement(in,magic);
    if (magic == 0x234235566ULL) {
      read_566(this,in); return;
    }
    
    if (magic != bum_magic)
      {
        if (magic != bum_magic_old)
          throw std::runtime_error("wrong magic number in umesh file ...");
        hasGrids = false;
      }
    io::readVector(in,this->vertices,"vertices");
    size_t numPerVertexAttributes = 1;
    io::readElement(in,numPerVertexAttributes);
    if (numPerVertexAttributes) {
      Attribute::SP attr = std::make_shared<Attribute>();
      io::readString(in,attr->name);
      io::readVector(in,attr->values,"scalars");
      attr->finalize();
      attributes.push_back(attr);
    }
    if (!attributes.empty())
      perVertex = attributes[0];
      
    size_t numPerElementAttributes = 0;
    io::readElement(in,numPerElementAttributes);
    assert(numPerElementAttributes == 0);
    
    io::readVector(in,this->triangles,"triangles");
    io::readVector(in,this->quads,"quads");
    io::readVector(in,this->tets,"tets");
    io::readVector(in,this->pyrs,"pyramids");
    io::readVector(in,this->wedges,"wedges");
    io::readVector(in,this->hexes,"hexes");
    if (hasGrids) {
      io::readVector(in,this->grids,"grids");
      io::readVector(in,this->gridScalars,"gridScalars");
    }
    // try {
    if (!in.eof())
      try {
        io::readVector(in,this->vertexTags,"vertexTags");
      } catch (...) {
        /* ignore ... */
      }
  
    this->finalize();
  }

  /*! read from given file, assuming file format as used by saveTo() */
  UMesh::SP UMesh::loadFrom(const std::string &fileName)
  {
    UMesh::SP mesh = std::make_shared<UMesh>();
    std::ifstream in(fileName, std::ios_base::binary);
    if (!in.good())
      throw std::runtime_error("#umesh: could not open '"+fileName+"'");
    mesh->readFrom(in);
    mesh->finalize();
    return mesh;
  }
  

  /*! tells this attribute that its values are set, and precomputations can be done */
  void Attribute::finalize()
  {
    std::mutex mutex;
    parallel_for_blocked
      (0,values.size(),16*1024,
       [&](size_t begin, size_t end) {
         range1f rangeValueRange;
         for (size_t i=begin;i<end;i++)
           rangeValueRange.extend(values[i]);
         std::lock_guard<std::mutex> lock(mutex);
         valueRange.extend(rangeValueRange.lower);
         valueRange.extend(rangeValueRange.upper);
       });
  }


  /*! create std::vector of primitmive references (bounding box plus
    tag) for every volumetric prim in this mesh */
  std::vector<UMesh::PrimRef> UMesh::createVolumePrimRefs()
  {
    std::vector<PrimRef> result;
    createVolumePrimRefs(result);
    return result;
  }


  /*! create std::vector of all primrefs for all _surface_ elements
    (triangles and quads) */
  std::vector<UMesh::PrimRef> UMesh::createSurfacePrimRefs()
  {
    std::vector<PrimRef> result;
    createSurfacePrimRefs(result);
    return (result);
  }
    

  /*! create std::vector of all primrefs for all _surface_ elements
    (triangles and quads) */
  void UMesh::createSurfacePrimRefs(std::vector<PrimRef> &result)
  {
    result.resize(triangles.size()+quads.size());
    parallel_for_blocked
      (0ull,triangles.size(),64*1024,
       [&](size_t begin,size_t end){
         for (size_t i=begin;i<end;i++)
           result[i] = PrimRef(TRI,i);
       });
    parallel_for_blocked
      (0ull,quads.size(),64*1024,
       [&](size_t begin,size_t end){
         for (size_t i=begin;i<end;i++)
           result[triangles.size()+i] = PrimRef(QUAD,i);
       });
  }
  
  /*! create std::vector of primitmive references (bounding box plus
    tag) for every volumetric prim in this mesh */
  void UMesh::createVolumePrimRefs(std::vector<UMesh::PrimRef> &result)
  {
    result.resize(tets.size()+pyrs.size()+wedges.size()+hexes.size()+grids.size());
    parallel_for_blocked
      (0ull,tets.size(),64*1024,
       [&](size_t begin,size_t end){
         for (size_t i=begin;i<end;i++)
           result[i] = PrimRef(TET,i);
       });
    parallel_for_blocked
      (0ull,pyrs.size(),64*1024,
       [&](size_t begin,size_t end){
         for (size_t i=begin;i<end;i++)
           result[tets.size()+i] = PrimRef(PYR,i);
       });
    parallel_for_blocked
      (0ull,wedges.size(),64*1024,
       [&](size_t begin,size_t end){
         for (size_t i=begin;i<end;i++)
           result[tets.size()+pyrs.size()+i] = PrimRef(WEDGE,i);
       });
    parallel_for_blocked
      (0ull,hexes.size(),64*1024,
       [&](size_t begin,size_t end){
         for (size_t i=begin;i<end;i++)
           result[tets.size()+pyrs.size()+wedges.size()+i] = PrimRef(HEX,i);
       });
    parallel_for_blocked
      (0ull,grids.size(),64*1024,
       [&](size_t begin,size_t end){
         for (size_t i=begin;i<end;i++)
           result[tets.size()+pyrs.size()+wedges.size()+hexes.size()+i] = PrimRef(GRID,i);
       });
  }


  /*! finalize a mesh, and compute min/max ranges where required */
  void UMesh::finalize()
  {
    if (perVertex) perVertex->finalize();
    bounds = box3f();

    std::vector<UMesh::PrimRef> allPrims = createAllPrimRefs();
    
    std::mutex mutex;
    parallel_for_blocked
      (0,allPrims.size(),16*1024,
       [&](size_t begin, size_t end) {
         box3f rangeBounds;
         for (size_t i=begin;i<end;i++)
           rangeBounds.extend(getBounds(allPrims[i]));
         std::lock_guard<std::mutex> lock(mutex);
         bounds.extend(rangeBounds);
       });

    gridsScalarRange = range1f();
    parallel_for_blocked
      (0,grids.size(),16*1024,
       [&](size_t begin, size_t end) {
         range1f rangeBounds;
         for (size_t i=begin;i<end;i++)
           rangeBounds.extend(getGridValueRange(i));
         std::lock_guard<std::mutex> lock(mutex);
         gridsScalarRange.extend(rangeBounds);
       });
  }
  
  /*! create std::vector of ALL primitmive references, includign
    both volume and surface ones */
  std::vector<UMesh::PrimRef> UMesh::createAllPrimRefs()
  {
    std::vector<PrimRef> allPRs = createVolumePrimRefs();
    for (auto prim : createSurfacePrimRefs())
      allPRs.push_back(prim);
    return allPRs;
  }

  /*! merge mulitple meshes into one. will _not_ try to find shared
    vertices, will just append all other elements and change their
    indices to correctly point to the appended other vertices */
  UMesh::SP mergeMeshes(const std::vector<UMesh::SP> &inputs)
  {
    size_t numVtx=0,
      numTet=0,
      numWdg=0,
      numPyr=0,
      numHex=0,
      numTri=0,
      numQud=0,
      numGsc=0,
      numGrd=0;
    std::vector<int> vtxOffsets;
    std::vector<int> tetOffsets;
    std::vector<int> triOffsets;
    std::vector<int> qudOffsets;
    std::vector<int> wdgOffsets;
    std::vector<int> pyrOffsets;
    std::vector<int> hexOffsets;
    std::vector<int> grdOffsets;
    std::vector<int> gscOffsets;
    for (auto input : inputs) {
      
      vtxOffsets.push_back((int)numVtx);
      triOffsets.push_back((int)numTri);
      qudOffsets.push_back((int)numQud);
      tetOffsets.push_back((int)numTet);
      pyrOffsets.push_back((int)numPyr);
      wdgOffsets.push_back((int)numWdg);
      hexOffsets.push_back((int)numHex);
      grdOffsets.push_back((int)numGrd);
      gscOffsets.push_back((int)numGsc);

      numVtx += input->vertices.size();
      if (numVtx > INT_MAX)
        throw std::runtime_error
          ("cannot merge meshes - merged mesh would have to "
           "many vertices to be addressable by 32-bit (signed) integers");
      numTri += input->triangles.size();
      numQud += input->quads.size();
      numTet += input->tets.size();
      numPyr += input->pyrs.size();
      numWdg += input->wedges.size();
      numHex += input->hexes.size();
      numGrd += input->grids.size();
      numGsc += input->gridScalars.size();
    }
    UMesh::SP out = std::make_shared<UMesh>();
    out->perVertex = std::make_shared<Attribute>();
    out->perVertex->values.resize(numVtx);
    out->vertices.resize(numVtx);
    out->triangles.resize(numTri);
    out->quads.resize(numQud);
    out->tets.resize(numTet);
    out->pyrs.resize(numPyr);
    out->wedges.resize(numWdg);
    out->hexes.resize(numHex);
    out->grids.resize(numGrd);
    out->gridScalars.resize(numGsc);
    
    serial_for
      ((int)inputs.size(),
       [&](int meshID) {
         auto input = inputs[meshID];
         for (int i=0;i<input->vertices.size();i++) {
           auto v = input->vertices[i];
           auto s = input->perVertex->values[i];
           out->vertices[vtxOffsets[meshID]+i] = v;
           out->perVertex->values[vtxOffsets[meshID]+i] = s;
         }

         for (int i=0;i<input->triangles.size();i++) {
           auto prim = input->triangles[i];
           for (int j=0;j<prim.numVertices;j++)
             prim[j] += vtxOffsets[meshID];
           out->triangles[triOffsets[meshID]+i] = prim;
         }
         for (int i=0;i<input->quads.size();i++) {
           auto prim = input->quads[i];
           for (int j=0;j<prim.numVertices;j++)
             prim[j] += vtxOffsets[meshID];
           out->quads[qudOffsets[meshID]+i] = prim;
         }
         for (int i=0;i<input->tets.size();i++) {
           auto prim = input->tets[i];
           for (int j=0;j<prim.numVertices;j++)
             prim[j] += vtxOffsets[meshID];
           out->tets[tetOffsets[meshID]+i] = prim;
         }
         for (int i=0;i<input->pyrs.size();i++) {
           auto prim = input->pyrs[i];
           for (int j=0;j<prim.numVertices;j++)
             prim[j] += vtxOffsets[meshID];
           out->pyrs[pyrOffsets[meshID]+i] = prim;
         }
         for (int i=0;i<input->wedges.size();i++) {
           auto prim = input->wedges[i];
           for (int j=0;j<prim.numVertices;j++)
             prim[j] += vtxOffsets[meshID];
           out->wedges[wdgOffsets[meshID]+i] = prim;
         }
         for (int i=0;i<input->hexes.size();i++) {
           auto prim = input->hexes[i];
           for (int j=0;j<prim.numVertices;j++)
             prim[j] += vtxOffsets[meshID];
           out->hexes[hexOffsets[meshID]+i] = prim;
         }
         for (int i=0;i<input->grids.size();i++) {
           auto prim = input->grids[i];
           prim.scalarsOffset += gscOffsets[meshID];
           out->grids[grdOffsets[meshID]+i] = prim;
         }
         for (int i=0;i<input->gridScalars.size();i++) {
           out->gridScalars[gscOffsets[meshID]+i] = input->gridScalars[i];
         }
       });
    out->finalize();
    return out;
  }
    
  
  /*! appends another mesh's vertices and primitmives to this
    current mesh. will _not_ try to find shared vertices, will
    just append all other elements and change their indices to
    correctly point to the appended other vertices */
  void UMesh::append(UMesh::SP other)
  {
    size_t oldNumVertices = vertices.size();
    // ----------- vertices -----------
    for (auto v : other->vertices)
      vertices.push_back(v);
    // ----------- scalars -----------
    if (perVertex) {
      assert(other->perVertex);
      for (auto v : other->perVertex->values)
        perVertex->values.push_back(v);
    }
    // ----------- tets -----------
    for (auto prim : other->tets) {
      for (int i=0;i<prim.numVertices;i++)
        prim[i] += (int)oldNumVertices;
      tets.push_back(prim);
    }
    // ----------- hexes -----------
    for (auto prim : other->hexes) {
      for (int i=0;i<prim.numVertices;i++)
        prim[i] += (int)oldNumVertices;
      hexes.push_back(prim);
    }
    // ----------- pyrs -----------
    for (auto prim : other->pyrs) {
      for (int i=0;i<prim.numVertices;i++)
        prim[i] += (int)oldNumVertices;
      pyrs.push_back(prim);
    }
    // ----------- wedges -----------
    for (auto prim : other->wedges) {
      for (int i=0;i<prim.numVertices;i++)
        prim[i] += (int)oldNumVertices;
      wedges.push_back(prim);
    }

    
    // ----------- grid-scalars -----------
    size_t oldNumGridScalars = gridScalars.size();
    for (auto s : other->gridScalars)
      gridScalars.push_back(s);
    // ----------- grids -----------
    for (auto prim : other->grids) {
      prim.scalarsOffset += (int)oldNumGridScalars;
      grids.push_back(prim);
    }


    // =========== done ===========
    finalize();
  }
    
  
  /*! print some basic info of this mesh to std::cout */
  void UMesh::print()
  {
    std::cout << toString(false);
  }
  
  /*! return a string of the form "UMesh{#tris=...}" */
  std::string UMesh::toString(bool compact) const
  {
    std::stringstream ss;

    if (compact) {
      ss << "UMesh(";
      ss << "#verts=" << prettyNumber(vertices.size());
      if (!triangles.empty())
        ss << ",#tris=" << prettyNumber(triangles.size());
      if (!quads.empty())
        ss << ",#quads=" << prettyNumber(quads.size());
      if (!tets.empty())
        ss << ",#tets=" << prettyNumber(tets.size());
      if (!pyrs.empty())
        ss << ",#pyrs=" << prettyNumber(pyrs.size());
      if (!wedges.empty())
        ss << ",#wedges=" << prettyNumber(wedges.size());
      if (!hexes.empty())
        ss << ",#hexes=" << prettyNumber(hexes.size());
      if (!grids.empty())
        ss << ",#grids=" << prettyNumber(grids.size())
           << " (with " << prettyNumber(gridScalars.size()) << " grid scalars)";
      if (perVertex) {
        ss << ",scalars=yes(name='" << perVertex->name << "')";
      } else {
        ss << ",scalars=no";
      }
      ss << "total attributes: " << attributes.size() << " (";
      for (int i=0;i<attributes.size();i++) {
        if (i) ss << ",";
        ss << "'" << attributes[i]->name << "'";
      }
      ss << ")";
      if (!vertexTags.empty()) {
        ss << ",tags=yes";
      } else {
        ss << ",tags=no";
      }
      ss << ")";
    } else {
      ss << "#verts : " << prettyNumber(vertices.size()) << std::endl;
      ss << "#tris  : " << prettyNumber(triangles.size()) << std::endl;
      ss << "#quads : " << prettyNumber(quads.size()) << std::endl;
      ss << "#tets  : " << prettyNumber(tets.size()) << std::endl;
      ss << "#pyrs  : " << prettyNumber(pyrs.size()) << std::endl;
      ss << "#wedges: " << prettyNumber(wedges.size()) << std::endl;
      ss << "#hexes : " << prettyNumber(hexes.size()) << std::endl;
      ss << "#grids : " << prettyNumber(grids.size())
 << " (with " << prettyNumber(gridScalars.size()) << " grid scalars)" << std::endl;
      if  (!bounds.empty())
        ss << "bounds : " << bounds << std::endl;
      if (perVertex) {
        range1f valueRange = getValueRange();
        if (valueRange.lower > valueRange.upper)
          ss << "values : yes (range not yet computed)" << std::endl;
        else
          ss << "values : " << valueRange << std::endl;
      } else
        ss << "values : <none>" << std::endl;
      ss << "tags : " << (vertexTags.empty()?"no":"yes") << std::endl;
      ss << "total attributes: " << attributes.size() <<  " (";
      for (int i=0;i<attributes.size();i++) {
        if (i) ss << ",";
        ss << "'" << attributes[i]->name << "'";
      }
      ss << ")";
    }
    return ss.str();
  }

  
} // ::tetty

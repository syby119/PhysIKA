﻿/*
* @file obj_mesh_io.cpp 
* @brief load and save mesh to an obj file.
* @author Fei Zhu
* 
* This file is part of Physika, a versatile physics simulation library.
* Copyright (C) 2013 Physika Group.
*
* This Source Code Form is subject to the terms of the GNU General Public License v2.0. 
* If a copy of the GPL was not distributed with this file, you can obtain one at:
* http://www.gnu.org/licenses/gpl-2.0.html
*
*/

#include <sstream>
#include <fstream>
#include "Physika_Core/Utilities/physika_assert.h"
#include "Physika_Geometry/Surface_Mesh/surface_mesh.h"
#include "Physika_IO/Surface_Mesh_IO/obj_mesh_io.h"
#include "Physika_Core/Utilities/File_Path_Utilities/File_Path_Utilities.h"

using Physika::SurfaceMeshInternal::Face;
using Physika::SurfaceMeshInternal::Group;
using Physika::SurfaceMeshInternal::Material;
using Physika::SurfaceMeshInternal::Vertex;

namespace Physika{
	
template <typename Scalar>
void ObjMeshIO<Scalar>::load(const string &filename, SurfaceMesh<Scalar> *mesh)
{
    PHYSIKA_MESSAGE_ASSERT(mesh,"invalid mesh point");
    string::size_type suffix_idx = filename.find('.');
	PHYSIKA_MESSAGE_ASSERT(suffix_idx < filename.size(), "this is not a obj file");
    string suffix = filename.substr(suffix_idx);
    if(suffix != string(".obj"))
	{
		PHYSIKA_ERROR("this is not a obj file");
	}		
	Group<Scalar>* current_group =NULL;
	unsigned int num_face=0;
	unsigned int current_material_index=0;
	unsigned int line_num =0;
    unsigned int num_group_faces = 0;
    std::string group_source_name;
    unsigned int group_clone_index = 0;
    std::fstream ifs( filename.c_str(),std::ios::in);
    if(!ifs)
    {
		PHYSIKA_ERROR("couldn't open .obj file");
	}
	const int maxline = 1000;
    char line[maxline];
    while(ifs){
        ++line_num;
		ifs.getline(line,maxline);
        std::stringstream stream;
        stream.clear();
		stream<<line;
        char type_of_line[50];
		stream>>ttype_of_line;
        if(strncmp(line,"v ",2) == 0)
		{
            //vertex
            Scalar x,y,z;
            PHYSIKA_MESSAGE_ASSERT(stream>>x, "x position of a vertice read error");
            PHYSIKA_MESSAGE_ASSERT(stream>>y, "y position of a vertice read error");
            PHYSIKA_MESSAGE_ASSERT(stream>>z, "z position of a vertice read error");
            mesh->addVertexPosition(Vector<Scalar,3>(x,y,z));
        }
        else if(strncmp(line, "vn ", 3) == 0)
		{   //vertex normal
            Scalar x,y,z;
            PHYSIKA_MESSAGE_ASSERT(stream>>x, "x position of a normal read error");
            PHYSIKA_MESSAGE_ASSERT(stream>>y, "y position of a normal read error");
            PHYSIKA_MESSAGE_ASSERT(stream>>z, "z position of a normal read error");
            mesh->addVertexNormal(Vector<Scalar,3>(x,y,z));
        }
        else if(strncmp(line, "vt", 3)==0)
		{   //vertex texture
            Scalar x,y;
            PHYSIKA_MESSAGE_ASSERT(stream>>x, "x position of a texture read error");
            PHYSIKA_MESSAGE_ASSERT(stream>>y, "y position of a texture read error");
            mesh->addVertexTextureCoordinate(Vector<Scalar,2>(x,y));
        }
        else if(strncmp(line, "g ", 2) == 0)
		{
			string group_name;
			stream>>group_name;
			PHYSIKA_MESSAGE_ASSERT(group_name.size() >= 1, "warning: empty group name come in"); 
            if(current_group = mesh->groupPtr(group_name))
			{
            }
            else
			{
                mesh->addGroup(Group<Scalar>(group_name,current_material_index));
                current_group=mesh->groupPtr(group_name);
                group_source_name=group_name;
                group_clone_index=0;
                num_group_faces=0;
            }

        }
        else if(strncmp(line, "f ",2) == 0|| (strncmp(line, "fo ", 3) == 0))
		{
            if(current_group==NULL)
			{
                mesh->addGroup(Group<Scalar>(string("default")));
                current_group=mesh->groupPtr(string("default"));
            }
            Face<Scalar> face_temple;
            char vertex_indice[20]={};
            while(stream>>vertex_indice)
			{
                unsigned int pos;
                unsigned int nor;
                unsigned int tex;
                if(strstr(vertex_indice,"//") != NULL)
				{   //    v//n
                    if(sscanf(vertex_indice,"%u//%u", &pos, &nor) == 2 )
					{
                        Vertex<Scalar> vertex_temple(pos-1);
                        vertex_temple.setNormalIndex(nor-1);
                        face_temple.addVertex(vertex_temple);
                    }
					else PHYSIKA_ERROR("invalid vertx in this face\n");
                }
                else
				{
                    if(sscanf(vertex_indice,"%u/%u/%u", &pos,&tex,&nor) != 3)
					{
                        if(strstr(vertex_indice,"/") != NULL)
						{    //  v/t
                            if(sscanf(vertex_indice, "%u/%u", &pos, &tex) == 2)
							{
                                Vertex<Scalar> vertex_temple(pos-1);
                                vertex_temple.setTextureCoordinateIndex(tex-1);
                                face_temple.addVertex(vertex_temple);
                            }
                            else
							{
                                PHYSIKA_ASSERT("%u/%u error");
                            }
                        }
                        else
						{
                            if(sscanf(vertex_indice,"%u", &pos) == 1)
							{
                                face_temple.addVertex(Vertex<Scalar>(pos-1));
                            }
                            else 
							{
                                PHYSIKA_ASSERT("%u error");
                            }
                        }
                    }
					else 
					{
                        Vertex<Scalar> vertex_temple(pos-1);
                        vertex_temple.setNormalIndex(nor-1);
                        vertex_temple.setTextureCoordinateIndex(tex-1);
                        face_temple.addVertex(vertex_temple);
                    }
                }
            }// end while vertex_indices
            num_face++;
            num_group_faces ++;
            current_group->addFace(face_temple);
		}
		else if((strncmp(line, "#", 1) == 0) || (strncmp(line, "\0", 1) == 0)){}
        else if(strncmp(line, "usemtl", 6) == 0)
		{
            if (num_group_faces > 0)
            {
                // usemtl without a "g" statement : must create a new group
                //first ,create unique name
				std::stringstream clone_name;
				clone_name.clear();
				clone_name<<group_source_name;
				clone_name<<'.';
				clone_name<<group_clone_index;
                std::string new_name;
				clone_name>>new_name;
                mesh->addGroup(Group<Scalar>(new_name));
                current_group=mesh->groupPtr(new_name);
                num_group_faces = 0;
                group_clone_index++;	
            }
			string material_name;
            stream>>material_name;
            if(current_material_index = (mesh->materialIndex(material_name)) != -1)
            {
                if(mesh->groups_.empty())
                {
                    groups.push_back(Group<Scalar>(string("default") ) );
                    current_group = mesh->groupPtr(string("default"));
                }
                current_group->setMaterialIndex(current_material_index);
            }
            else {PHYSIKA_ASSERT("material found false");}
        }
        else if(strncmp(line, "mtllib", 6) == 0)
        {
            string mtl_name;
            stream>>mtl_name;
			string pre_path = File_Path_Utilities::dirname(filename);
			mtl_name=pre_path+ string("/") +mtl_name;
            loadMaterials(mtl_name,mesh);
        }
        else 
        {
            // do nothing
        }		
    }//end while
    ifs.close();
    //cout some file message
}

template <typename Scalar>
void ObjMeshIO<Scalar>::save(const string &filename, SurfaceMesh<Scalar> *mesh)
{
    //TO DO: implementation
	PHYSIKA_MESSAGE_ASSERT(mesh, "invalid mesh point");
    string::size_type suffix_idx = filename.find('.');
	PHYSIKA_MESSAGE_ASSERT(suffix_idx < filename.size(), "this is not a obj file");
	string suffix = filename.substr(suffix_idx), prefix = filename.substr(0, suffix_idx);
    if(suffix != string(".obj"))
	{
		PHYSIKA_ERROR("this is not a obj file");
	}	
	std::fstream fileout(filename.c_str(),std::ios::out|std::ios::trunc);
	PHYSIKA_MESSAGE_ASSERT(fileout, "fail to open file when save a mesh to a obj file");
	string material_path = prefix + string(".mtl");
	saveMaterials(material_path, mesh);
	fileout<<"mtllib "<<File_Path_Utilities::filenameInPath(prefix)<<".mtl"<<endl;
	unsigned int num_vertices=mesh->numVertices(),i;
	for(i=0;i<num_vertices;++i)
	{
		Vector<Scalar,3> example = mesh->vertexPosition(i);
		fileout<<"v "<<example[0]<<' '<<example[1]<<' '<<example[2]<<endl;
	}
	unsigned int num_vertex_normal = mesh->numNormals();
	for(i=0;i<num_vertex_normal;++i)
	{
		Vector<Scalar,3> example = mesh->vertexNormal(i);
		fileout<<"vn "<<example[0]<<' '<<example[1]<<' '<<example[2]<<endl;
	}
	unsigned int num_tex = mesh->numTextureCoordinates();
	for(i=0;i<num_tex;++i)
	{
		Vector<Scalar,2> example = mesh->vertexTextureCoordinate(i);
		fileout<<"vt "<<example[0]<<' '<<example[1]<<endl;
	}
	unsigned int num_group = mesh->numGroups();
	for(i=0;i<num_group;++i)
	{
		Group<Scalar> *group_ptr = mesh->groupPtr(i);
		fileout<<"usemtl "<<mesh->materialPtr(group_ptr->materialIndex())->name()<<endl;
		fileout<<"g "<<group_ptr->name()<<endl;
		unsigned int num_face = group_ptr->numFaces(),j;
		Face<Scalar> *face_ptr;
		for(j=0;j<num_face;++j)
		{
			face_ptr = group_ptr->facePtr(j);
			fileout<<"f ";
			unsigned int num_vertices_inface = face_ptr->numVertices(),k;
			Vertex<Scalar> *vertex_ptr;
			for(k=0;k<num_vertices_inface;++k)
			{
				vertex_ptr = face_ptr->vertexPtr(k);
				file<<vertex_ptr->positionIndex()+1;
				if(vertex_ptr->hasTexture()||vertex_ptr->hasNormal())
				{
					fileout<<'/';
					if(vertex_ptr->hasTexture()) fileout<<vertex_ptr->textureCoordinateIndex()+1;
				}
				if(vertex_ptr->hasNormal()) fileout<<'/'<<vertex_ptr->normalIndex()+1<<' ';
			}
			fileout<<endl;
		}
	}
    fileout.close();
}

template <typename Scalar>
void ObjMeshIO<Scalar>::loadMaterials(const string &filename, SurfaceMesh<Scalar> *mesh)
{
	std::fstream ifs(filename.c_str(),std::ios::in);
	PHYSIKA_MESSAGE_ASSERT(ifs, "can't open this mtl file");
	const unsigned int maxline = 1024;
	char line[maxline];
	char prefix[maxline];
	std::stringstream stream;
	unsigned int num_mtl=0;
	Material<Scalar> material_example;
	while(ifs)
	{		
		ifs.getline(line, maxline);
		stream.clear();
		switch (line[0])
		{
		case '#':
			break;

		case 'n':
			if(num_mtl > 0) mesh->addMaterial(material_example);
			material_example.setKa(Vector<Scalar> (0.1, 0.1, 0.1));
			material_example.setKd(Vector<Scalar> (0.5, 0.5, 0.5));
			material_example.setKs(Vector<Scalar> (0.0, 0.0, 0.0));
			material_example.setShininess(65);
			material_example.setTextureFileName(string());
			char mtl_name[maxline];
			sscanf(line, "%s %s", prefix, mtl_name);
			num_mtl++;
			material_example.setName(string(mtl_name));
			break;

		case 'N':
			if (line[1] == 's')
			{
				Scalar shininess;
				stream<<line;
				stream>>prefix;
				PHYSIKA_MESSAGE_ASSERT(stream>>shininess, "error! no data to set shininess");
				shininess *= 128.0 /1000.0;
				material_example.setShininess(shininess);
			}
			else {}
			break;

		case 'K':
			switch (line[1])
			{
			case 'd':
				Scalar kd1,kd2,kd3;
				stream<<line;
				stream>>prefix;
				if(!(stream>>kd1)) break;
				stream>>kd2;
				PHYSIKA_MESSAGE_ASSERT(stream>>kd3, "error less data when read Kd.\n");
				material_example.setKd(Vector<Scalar> (kd1, kd2, kd3));
				break;

			case 's':
				Scalar ks1,ks2,ks3;
				stream<<line;
				stream>>prefix;
				if(!(stream>>ks1)) break;
				stream>>ks2;
				PHYSIKA_MESSAGE_ASSERT(stream>>ks3, "error less data when read Ks.\n");
				material_example.setKs(Vector<Scalar> (ks1, ks2, ks3));
				break;

			case 'a':
				Scalar ka1,ka2,ka3;
				stream<<line;
				stream>>prefix;
				if(!(stream>>ka1)) break;
				stream>>ka2;
				PHYSIKA_MESSAGE_ASSERT(stream>>ka3, "error less data when read Ka.\n");
				material_example.setKa(Vector<Scalar> (ka1, ka2, ka3));
				break;

			default:
				break;
			}

		case 'm':
			char tex_name[maxline];
			stream<<line;
			stream>>prefix;
			stream>>tex_name;
			material_example.setTextureFileName(string(tex_name));
			break;

		case 'd':
			char next[maxline];
			stream<<line;
			stream>>prefix;
			Scalar alpha;
			stream>>next;
			if(next[0] == '-') stream>>aalpha;
			else 
			{
				stream.clear();
				stream<<next;
				stream>>alpha;
			}
			material_example.setAlpha(alpha);
			break;

		default:
			break;
		}
	}
	if(num_mtl >= 0)                            //attention at least one material must be in mesh
		mesh->addMaterial(material_example);
	ifs.close();
}

template <typename Scalar>
void ObjMeshIO<Scalar>::saveMaterials(const string &filename, SurfaceMesh<Scalar> *mesh)
{
	std::fstream fileout(filename.c_str(),std::ios::out|std::ios::trunc);
	PHYSIKA_MESSAGE_ASSERT(fileout, "error:can't open file when save materials.");
	PHYSIKA_MESSAGE_ASSERT(mesh, "error:invalid mesh point.");
	unsigned int num_mtl = mesh->numMaterials();
	unsigned int i;
	Material<Scalar> material_example;
	for(i = 0;i < num_mtl;i++)
	{
		material_example = mesh->material(i);
		fileout<<"newmtl "<<material_example.name()<<endl;
		fileout<<"Ka "<<material_example.Ka()[0]<<' '<<material_example.Ka()[1]<<' '<<material_example.Ka()[2]<<endl;
		fileout<<"Kd "<<material_example.Kd()[0]<<' '<<material_example.Kd()[1]<<' '<<material_example.Kd()[2]<<endl;
		fileout<<"Ks "<<material_example.Ks()[0]<<' '<<material_example.Ks()[1]<<' '<<material_example.Ks()[2]<<endl;
		fileout<<"Ns "<<material_example.shininess()*1000.0/128.0<<endl;
		fileout<<"d "<<material_example.alpha()<<endl;
		if(material_example.hasTexture()) fileout<<"map_Ka "<<material_example.textureFileName()<<endl;
	}
	fileout.close();
}

//explicit instantitation
template class ObjMeshIO<float>;
template class ObjMeshIO<double>;

} //end of namespace Physika



















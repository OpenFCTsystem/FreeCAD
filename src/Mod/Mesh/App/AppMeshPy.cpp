/***************************************************************************
 *   Copyright (c) 2004 Werner Mayer <wmayer[at]users.sourceforge.net>     *
 *                                                                         *
 *   This file is part of the FreeCAD CAx development system.              *
 *                                                                         *
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Library General Public           *
 *   License as published by the Free Software Foundation; either          *
 *   version 2 of the License, or (at your option) any later version.      *
 *                                                                         *
 *   This library  is distributed in the hope that it will be useful,      *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU Library General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this library; see the file COPYING.LIB. If not,    *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,         *
 *   Suite 330, Boston, MA  02111-1307, USA                                *
 *                                                                         *
 ***************************************************************************/

#include "PreCompiled.h"
#ifndef _PreComp_
# include <algorithm>
#endif

#include <CXX/Extensions.hxx>
#include <CXX/Objects.hxx>

#include <Base/Console.h>
#include <Base/Interpreter.h>
#include <Base/FileInfo.h>
#include <Base/Tools.h>
#include <App/Application.h>
#include <App/Document.h>
#include <App/DocumentObjectPy.h>
#include <App/Property.h>
#include <Base/PlacementPy.h>

#include <Base/GeometryPyCXX.h>
#include <Base/VectorPy.h>

#include "Core/MeshKernel.h"
#include "Core/MeshIO.h"
#include "Core/Evaluation.h"
#include "Core/Iterator.h"
#include "Core/Approximation.h"

#include "WildMagic4/Wm4ContBox3.h"

#include "Mesh.h"
#include "FeatureMeshImport.h"
#include <Mod/Mesh/App/MeshPy.h>

using namespace Mesh;
using namespace MeshCore;

namespace Mesh {
class Module : public Py::ExtensionModule<Module>
{
    struct add_offset {
        unsigned long i;
        add_offset(unsigned long i) : i(i)
        {
        }
        void operator()(unsigned long& v)
        {
            v += i;
        }
    };
public:
    Module() : Py::ExtensionModule<Module>("Mesh")
    {
        add_varargs_method("read",&Module::read,
            "Read a mesh from a file and returns a Mesh object."
        );
        add_varargs_method("open",&Module::open,
            "open(string) -- Create a new document and a Mesh::Import feature to load the file into the document."
        );
        add_varargs_method("insert",&Module::importer,
            "insert(string|mesh,[string]) -- Load or insert a mesh into the given or active document."
        );
        add_varargs_method("export",&Module::exporter,
            "export(list,string,[tolerance]) -- Export a list of objects into a single file.  tolerance is in mm\n"
            "and specifies the maximum acceptable deviation between the specified objects and the exported mesh."
        );
        add_varargs_method("show",&Module::show,
            "Put a mesh object in the active document or creates one if needed"
        );
        add_varargs_method("createBox",&Module::createBox,
            "Create a solid mesh box"
        );
        add_varargs_method("createPlane",&Module::createPlane,
            "Create a mesh XY plane normal +Z"
        );
        add_varargs_method("createSphere",&Module::createSphere,
            "Create a tessellated sphere"
        );
        add_varargs_method("createEllipsoid",&Module::createEllipsoid,
            "Create a tessellated ellipsoid"
        );
        add_varargs_method("createCylinder",&Module::createCylinder,
            "Create a tessellated cylinder"
        );
        add_varargs_method("createCone",&Module::createCone,
            "Create a tessellated cone"
        );
        add_varargs_method("createTorus",&Module::createTorus,
            "Create a tessellated torus"
        );
        add_varargs_method("calculateEigenTransform",&Module::calculateEigenTransform,
            "calculateEigenTransform(seq(Base.Vector)) -- Calculates the eigen Transformation from a list of points.\n"
            "calculate the point's local coordinate system with the center\n"
            "of gravity as origin. The local coordinate system is computed\n"
            "this way that u has minimum and w has maximum expansion.\n"
            "The local coordinate system is right-handed.\n"
        );
        add_varargs_method("polynomialFit",&Module::polynomialFit,
            "polynomialFit(seq(Base.Vector)) -- Calculates a polynomial fit."
        );
        add_varargs_method("minimumVolumeOrientedBox",&Module::minimumVolumeOrientedBox,
            "minimumVolumeOrientedBox(seq(Base.Vector)) -- Calculates the minimum volume oriented box containing all points.\n"
            "The return value is a tuple of seven items: center, u, v, w directions and the lengths of the three vectors."
        );
        initialize("The functions in this module allow working with mesh objects.\n"
                   "A set of functions are provided that allow to read in registered mesh file formats\n"
                   "to either an newly created or already exising document.\n"
                   "\n"
                   "open(string) -- Create a new document and a Mesh::Import feature to load the file into the document.\n"
                   "insert(string, string) -- Create a Mesh::Import feature to load the file into the given document.\n"
                   "Mesh() -- Create an empty mesh object.\n"
                   "\n");
    }

    virtual ~Module() {}

private:
    virtual Py::Object invoke_method_varargs(void *method_def, const Py::Tuple &args)
    {
        try {
            return Py::ExtensionModule<Module>::invoke_method_varargs(method_def, args);
        }
        catch (const Base::Exception &e) {
            throw Py::RuntimeError(e.what());
        }
        catch (const std::exception &e) {
            throw Py::RuntimeError(e.what());
        }
    }
    Py::Object read(const Py::Tuple& args)
    {
        char* Name;
        if (!PyArg_ParseTuple(args.ptr(), "et","utf-8",&Name))
            throw Py::Exception();
        std::string EncodedName = std::string(Name);
        PyMem_Free(Name);

        std::unique_ptr<MeshObject> mesh(new MeshObject);
        mesh->load(EncodedName.c_str());
        return Py::asObject(new MeshPy(mesh.release()));
    }
    Py::Object open(const Py::Tuple& args)
    {
        char* Name;
        if (!PyArg_ParseTuple(args.ptr(), "et","utf-8",&Name))
            throw Py::Exception();

        std::string EncodedName = std::string(Name);
        PyMem_Free(Name);

        MeshObject mesh;
        MeshCore::Material mat;
        if (mesh.load(EncodedName.c_str(), &mat)) {
            Base::FileInfo file(EncodedName.c_str());
            // create new document and add Import feature
            App::Document *pcDoc = App::GetApplication().newDocument("Unnamed");
            unsigned long segmct = mesh.countSegments();
            if (segmct > 1) {
                for (unsigned long i=0; i<segmct; i++) {
                    const Segment& group = mesh.getSegment(i);
                    std::string groupName = group.getName();
                    if (groupName.empty())
                        groupName = file.fileNamePure();

                    std::unique_ptr<MeshObject> segm(mesh.meshFromSegment(group.getIndices()));
                    Mesh::Feature *pcFeature = static_cast<Mesh::Feature *>
                        (pcDoc->addObject("Mesh::Feature", groupName.c_str()));
                    pcFeature->Label.setValue(groupName.c_str());
                    pcFeature->Mesh.swapMesh(*segm);
                    pcFeature->purgeTouched();
                }
            }
            else if (mat.binding == MeshCore::MeshIO::PER_VERTEX && 
                     mat.diffuseColor.size() == mesh.countPoints()) {
                FeatureCustom *pcFeature = new FeatureCustom();
                pcFeature->Label.setValue(file.fileNamePure().c_str());
                pcFeature->Mesh.swapMesh(mesh);
                App::PropertyColorList* prop = static_cast<App::PropertyColorList*>
                    (pcFeature->addDynamicProperty("App::PropertyColorList", "VertexColors"));
                if (prop) {
                    prop->setValues(mat.diffuseColor);
                }
                pcFeature->purgeTouched();

                pcDoc->addObject(pcFeature, file.fileNamePure().c_str());
            }
            else {
                Mesh::Feature *pcFeature = static_cast<Mesh::Feature *>
                    (pcDoc->addObject("Mesh::Feature", file.fileNamePure().c_str()));
                pcFeature->Label.setValue(file.fileNamePure().c_str());
                pcFeature->Mesh.swapMesh(mesh);
                pcFeature->purgeTouched();
            }
        }

        return Py::None();
    }
    Py::Object importer(const Py::Tuple& args)
    {
        char* Name;
        char* DocName=0;
        if (!PyArg_ParseTuple(args.ptr(), "et|s","utf-8",&Name,&DocName))
            throw Py::Exception();

        std::string EncodedName = std::string(Name);
        PyMem_Free(Name);

        App::Document *pcDoc = 0;
        if (DocName)
            pcDoc = App::GetApplication().getDocument(DocName);
        else
            pcDoc = App::GetApplication().getActiveDocument();

        if (!pcDoc) {
            pcDoc = App::GetApplication().newDocument(DocName);
        }

        MeshObject mesh;
        MeshCore::Material mat;
        if (mesh.load(EncodedName.c_str(), &mat)) {
            Base::FileInfo file(EncodedName.c_str());
            unsigned long segmct = mesh.countSegments();
            if (segmct > 1) {
                for (unsigned long i=0; i<segmct; i++) {
                    const Segment& group = mesh.getSegment(i);
                    std::string groupName = group.getName();
                    if (groupName.empty())
                        groupName = file.fileNamePure();

                    std::unique_ptr<MeshObject> segm(mesh.meshFromSegment(group.getIndices()));
                    Mesh::Feature *pcFeature = static_cast<Mesh::Feature *>
                        (pcDoc->addObject("Mesh::Feature", groupName.c_str()));
                    pcFeature->Label.setValue(groupName.c_str());
                    pcFeature->Mesh.swapMesh(*segm);
                    pcFeature->purgeTouched();
                }
            }
            else if (mat.binding == MeshCore::MeshIO::PER_VERTEX && 
                     mat.diffuseColor.size() == mesh.countPoints()) {
                FeatureCustom *pcFeature = new FeatureCustom();
                pcFeature->Label.setValue(file.fileNamePure().c_str());
                pcFeature->Mesh.swapMesh(mesh);
                App::PropertyColorList* prop = static_cast<App::PropertyColorList*>
                    (pcFeature->addDynamicProperty("App::PropertyColorList", "VertexColors"));
                if (prop) {
                    prop->setValues(mat.diffuseColor);
                }
                pcFeature->purgeTouched();

                pcDoc->addObject(pcFeature, file.fileNamePure().c_str());
            }
            else {
                Mesh::Feature *pcFeature = static_cast<Mesh::Feature *>
                    (pcDoc->addObject("Mesh::Feature", file.fileNamePure().c_str()));
                pcFeature->Label.setValue(file.fileNamePure().c_str());
                pcFeature->Mesh.swapMesh(mesh);
                pcFeature->purgeTouched();
            }
        }

        return Py::None();
    }
    Py::Object exporter(const Py::Tuple& args)
    {
        PyObject *object;
        char *Name;

        // If tolerance is specified via python interface, use that.
        // If not, use the preference, if that exists, else default to 0.1mm.
        ParameterGrp::handle hGrp = App::GetApplication().GetParameterGroupByPath("User parameter:BaseApp/Preferences/Mod/Mesh");
        float fTolerance = hGrp->GetFloat( "MaxDeviationExport", 0.1f );

        if (!PyArg_ParseTuple(args.ptr(), "Oet|f", &object, "utf-8", &Name, &fTolerance))
            throw Py::Exception();

        std::string EncodedName = std::string(Name);
        PyMem_Free(Name);

        MeshObject global_mesh;

        Py::Sequence list(object);
        Base::Type meshId = Base::Type::fromName("Mesh::Feature");
        Base::Type partId = Base::Type::fromName("Part::Feature");
        for (Py::Sequence::iterator it = list.begin(); it != list.end(); ++it) {
            PyObject* item = (*it).ptr();
            if (PyObject_TypeCheck(item, &(App::DocumentObjectPy::Type))) {
                App::DocumentObject* obj = static_cast<App::DocumentObjectPy*>(item)->getDocumentObjectPtr();
                if (obj->getTypeId().isDerivedFrom(meshId)) {
                    const MeshObject& mesh = static_cast<Mesh::Feature*>(obj)->Mesh.getValue();
                    MeshCore::MeshKernel kernel = mesh.getKernel();
                    kernel.Transform(mesh.getTransform());

                    unsigned long countFacets = global_mesh.countFacets();
                    if (countFacets == 0)
                        global_mesh.setKernel(kernel);
                    else
                        global_mesh.addMesh(kernel);

                    // if the mesh already has persistent segments then use them instead
                    unsigned long numSegm = mesh.countSegments();
                    unsigned long canSave = 0;
                    for (unsigned long i=0; i<numSegm; i++) {
                        if (mesh.getSegment(i).isSaved())
                            canSave++;
                    }

                    if (canSave > 0) {
                        for (unsigned long i=0; i<numSegm; i++) {
                            const Segment& segm = mesh.getSegment(i);
                            if (segm.isSaved()) {
                                std::vector<unsigned long> indices = segm.getIndices();
                                std::for_each(indices.begin(), indices.end(), add_offset(countFacets));
                                Segment new_segm(&global_mesh, indices, true);
                                new_segm.setName(segm.getName());
                                global_mesh.addSegment(new_segm);
                            }
                        }

                    }
                    else {
                        // now create a segment for the added mesh
                        std::vector<unsigned long> indices;
                        indices.resize(global_mesh.countFacets() - countFacets);
                        std::generate(indices.begin(), indices.end(), Base::iotaGen<unsigned long>(countFacets));
                        Segment segm(&global_mesh, indices, true);
                        segm.setName(obj->Label.getValue());
                        global_mesh.addSegment(segm);
                    }
                }
                else if (obj->getTypeId().isDerivedFrom(partId)) {
                    App::Property* shape = obj->getPropertyByName("Shape");
                    Base::Reference<MeshObject> mesh(new MeshObject());
                    if (shape && shape->getTypeId().isDerivedFrom(App::PropertyComplexGeoData::getClassTypeId())) {
                        std::vector<Base::Vector3d> aPoints;
                        std::vector<Data::ComplexGeoData::Facet> aTopo;
                        const Data::ComplexGeoData* data = static_cast<App::PropertyComplexGeoData*>(shape)->getComplexData();
                        if (data) {
                            data->getFaces(aPoints, aTopo, fTolerance);
                            mesh->addFacets(aTopo, aPoints);

                            unsigned long countFacets = global_mesh.countFacets();
                            if (countFacets == 0)
                                global_mesh = *mesh;
                            else
                                global_mesh.addMesh(*mesh);

                            // now create a segment for the added mesh
                            std::vector<unsigned long> indices;
                            indices.resize(global_mesh.countFacets() - countFacets);
                            std::generate(indices.begin(), indices.end(), Base::iotaGen<unsigned long>(countFacets));
                            Segment segm(&global_mesh, indices, true);
                            segm.setName(obj->Label.getValue());
                            global_mesh.addSegment(segm);
                        }
                    }
                }
                else {
                    Base::Console().Message("'%s' is not a mesh or shape, export will be ignored.\n", obj->Label.getValue());
                }
            }
        }

        // if we have more than one segment set the 'save' flag
        if (global_mesh.countSegments() > 1) {
            for (unsigned long i = 0; i < global_mesh.countSegments(); ++i) {
                global_mesh.getSegment(i).save(true);
            }
        }
        // export mesh compound
        global_mesh.save(EncodedName.c_str());

        return Py::None();
    }
    Py::Object show(const Py::Tuple& args)
    {
        PyObject *pcObj;
        if (!PyArg_ParseTuple(args.ptr(), "O!", &(MeshPy::Type), &pcObj))
            throw Py::Exception();

        App::Document *pcDoc = App::GetApplication().getActiveDocument();
        if (!pcDoc)
            pcDoc = App::GetApplication().newDocument();
        MeshPy* pMesh = static_cast<MeshPy*>(pcObj);
        Mesh::Feature *pcFeature = (Mesh::Feature *)pcDoc->addObject("Mesh::Feature", "Mesh");
        Mesh::MeshObject* mo = pMesh->getMeshObjectPtr();
        if (!mo) {
            throw Py::Exception(PyExc_ReferenceError, "object doesn't reference a valid mesh");
        }
        // copy the data
        pcFeature->Mesh.setValue(*mo);

        return Py::None();
    }
    Py::Object createBox(const Py::Tuple& args)
    {
        float length = 10.0f;
        float width = 10.0f;
        float height = 10.0f;
        float edgelen = -1.0f;
        if (!PyArg_ParseTuple(args.ptr(), "|ffff",&length,&width,&height,&edgelen))
            throw Py::Exception();

        MeshObject* mesh;
        if (edgelen < 0.0f)
            mesh = MeshObject::createCube(length, width, height);
        else
            mesh = MeshObject::createCube(length, width, height, edgelen);

        if (!mesh) {
            throw Py::Exception(Base::BaseExceptionFreeCADError, "Creation of box failed");
        }
        return Py::asObject(new MeshPy(mesh));
    }
    Py::Object createPlane(const Py::Tuple& args)
    {
        float x=1,y=0,z=0;
        if (!PyArg_ParseTuple(args.ptr(), "|fff",&x,&y,&z))
            throw Py::Exception();

        if (y==0) 
            y=x;

        float hx = x/2.0f;
        float hy = y/2.0f;

        std::vector<MeshCore::MeshGeomFacet> TriaList;
        TriaList.push_back(MeshCore::MeshGeomFacet(Base::Vector3f(-hx, -hy, 0.0),Base::Vector3f(hx, hy, 0.0),Base::Vector3f(-hx, hy, 0.0)));
        TriaList.push_back(MeshCore::MeshGeomFacet(Base::Vector3f(-hx, -hy, 0.0),Base::Vector3f(hx, -hy, 0.0),Base::Vector3f(hx, hy, 0.0)));

        std::unique_ptr<MeshObject> mesh(new MeshObject);
        mesh->addFacets(TriaList);
        return Py::asObject(new MeshPy(mesh.release()));
    }
    Py::Object createSphere(const Py::Tuple& args)
    {
        float radius = 5.0f;
        int sampling = 50;
        if (!PyArg_ParseTuple(args.ptr(), "|fi",&radius,&sampling))
            throw Py::Exception();

        MeshObject* mesh = MeshObject::createSphere(radius, sampling);
        if (!mesh) {
            throw Py::Exception(Base::BaseExceptionFreeCADError, "Creation of sphere failed");
        }
        return Py::asObject(new MeshPy(mesh));
    }
    Py::Object createEllipsoid(const Py::Tuple& args)
    {
        float radius1 = 2.0f;
        float radius2 = 4.0f;
        int sampling = 50;
        if (!PyArg_ParseTuple(args.ptr(), "|ffi",&radius1,&radius2,&sampling))
            throw Py::Exception();

        MeshObject* mesh = MeshObject::createEllipsoid(radius1, radius2, sampling);
        if (!mesh) {
            throw Py::Exception(Base::BaseExceptionFreeCADError, "Creation of ellipsoid failed");
        }
        return Py::asObject(new MeshPy(mesh));
    }
    Py::Object createCylinder(const Py::Tuple& args)
    {
        float radius = 2.0f;
        float length = 10.0f;
        int closed = 1;
        float edgelen = 1.0f;
        int sampling = 50;
        if (!PyArg_ParseTuple(args.ptr(), "|ffifi",&radius,&length,&closed,&edgelen,&sampling))
            throw Py::Exception();

        MeshObject* mesh = MeshObject::createCylinder(radius, length, closed, edgelen, sampling);
        if (!mesh) {
            throw Py::Exception(Base::BaseExceptionFreeCADError, "Creation of cylinder failed");
        }
        return Py::asObject(new MeshPy(mesh));
    }
    Py::Object createCone(const Py::Tuple& args)
    {
        float radius1 = 2.0f;
        float radius2 = 4.0f;
        float len = 10.0f;
        int closed = 1;
        float edgelen = 1.0f;
        int sampling = 50;
        if (!PyArg_ParseTuple(args.ptr(), "|fffifi",&radius1,&radius2,&len,&closed,&edgelen,&sampling))
            throw Py::Exception();

        MeshObject* mesh = MeshObject::createCone(radius1, radius2, len, closed, edgelen, sampling);
        if (!mesh) {
            throw Py::Exception(Base::BaseExceptionFreeCADError, "Creation of cone failed");
        }
        return Py::asObject(new MeshPy(mesh));
    }
    Py::Object createTorus(const Py::Tuple& args)
    {
        float radius1 = 10.0f;
        float radius2 = 2.0f;
        int sampling = 50;
        if (!PyArg_ParseTuple(args.ptr(), "|ffi",&radius1,&radius2,&sampling))
            throw Py::Exception();

        MeshObject* mesh = MeshObject::createTorus(radius1, radius2, sampling);
        if (!mesh) {
            throw Py::Exception(Base::BaseExceptionFreeCADError, "Creation of torus failed");
        }
        return Py::asObject(new MeshPy(mesh));
    }
    Py::Object calculateEigenTransform(const Py::Tuple& args)
    {
        PyObject *input;

        if (!PyArg_ParseTuple(args.ptr(), "O",&input))
            throw Py::Exception();

        if (!PySequence_Check(input)) {
            throw Py::TypeError("Input has to be a sequence of Base.Vector()");
        }

        MeshCore::MeshKernel aMesh;
        MeshCore::MeshPointArray vertices;
        vertices.clear();
        MeshCore::MeshFacetArray faces;
        faces.clear();
        MeshCore::MeshPoint current_node;

        Py::Sequence list(input);
        for (Py::Sequence::iterator it = list.begin(); it != list.end(); ++it) {
            PyObject* value = (*it).ptr();
            if (PyObject_TypeCheck(value, &(Base::VectorPy::Type))) {
                Base::VectorPy  *pcObject = static_cast<Base::VectorPy*>(value);
                Base::Vector3d* val = pcObject->getVectorPtr();


                current_node.Set(float(val->x),float(val->y),float(val->z));
                vertices.push_back(current_node);
            }
        }

        MeshCore::MeshFacet aFacet;
        aFacet._aulPoints[0] = 0;aFacet._aulPoints[1] = 1;aFacet._aulPoints[2] = 2;
        faces.push_back(aFacet);
        //Fill the Kernel with the temp mesh structure and delete the current containers
        aMesh.Adopt(vertices,faces);
        MeshCore::MeshEigensystem pca(aMesh);
        pca.Evaluate();
        Base::Matrix4D Trafo = pca.Transform();

        return Py::asObject(new Base::PlacementPy(new Base::Placement(Trafo)));
    }
    Py::Object polynomialFit(const Py::Tuple& args)
    {
        PyObject *input;

        if (!PyArg_ParseTuple(args.ptr(), "O",&input))
            throw Py::Exception();

        if (!PySequence_Check(input)) {
            throw Py::TypeError("Input has to be a sequence of Base.Vector()");
        }

        MeshCore::SurfaceFit polyFit;

        Base::Vector3f point;
        Py::Sequence list(input);
        for (Py::Sequence::iterator it = list.begin(); it != list.end(); ++it) {
            PyObject* value = (*it).ptr();
            if (PyObject_TypeCheck(value, &(Base::VectorPy::Type))) {
                Base::VectorPy  *pcObject = static_cast<Base::VectorPy*>(value);
                Base::Vector3d* val = pcObject->getVectorPtr();
                point.Set(float(val->x),float(val->y),float(val->z));
                polyFit.AddPoint(point);
            }
        }

        // fit quality
        float fit = polyFit.Fit();
        Py::Dict dict;
        dict.setItem(Py::String("Sigma"), Py::Float(fit));

        // coefficients
        double a,b,c,d,e,f;
        polyFit.GetCoefficients(a,b,c,d,e,f);
        Py::Tuple p(6);
        p.setItem(0, Py::Float(a));
        p.setItem(1, Py::Float(b));
        p.setItem(2, Py::Float(c));
        p.setItem(3, Py::Float(d));
        p.setItem(4, Py::Float(e));
        p.setItem(5, Py::Float(f));
        dict.setItem(Py::String("Coefficients"), p);

        // residuals
        std::vector<Base::Vector3f> local = polyFit.GetLocalPoints();
        Py::Tuple r(local.size());
        for (std::vector<Base::Vector3f>::iterator it = local.begin(); it != local.end(); ++it) {
            double z = polyFit.Value(it->x, it->y);
            double d = it->z - z;
            r.setItem(it-local.begin(), Py::Float(d));
        }
        dict.setItem(Py::String("Residuals"), r);

        return dict;
    }
    Py::Object minimumVolumeOrientedBox(const Py::Tuple& args) {
        PyObject *input;

        if (!PyArg_ParseTuple(args.ptr(), "O",&input))
            throw Py::Exception();

        if (!PySequence_Check(input)) {
            throw Py::TypeError("Input has to be a sequence of Base.Vector()");
        }

        Py::Sequence list(input);
        std::vector<Wm4::Vector3d> points;
        points.reserve(list.size());
        for (Py::Sequence::iterator it = list.begin(); it != list.end(); ++it) {
            PyObject* value = (*it).ptr();
            if (PyObject_TypeCheck(value, &(Base::VectorPy::Type))) {
                Base::VectorPy  *pcObject = static_cast<Base::VectorPy*>(value);
                Base::Vector3d* val = pcObject->getVectorPtr();
                Wm4::Vector3d pt;
                pt[0] = val->x;
                pt[1] = val->y;
                pt[2] = val->z;
                points.push_back(pt);
            }
        }

        if (points.size() < 4)
            throw Py::RuntimeError("Too few points");

        Wm4::Box3d mobox = Wm4::ContMinBox(points.size(), &(points[0]), 0.001, Wm4::Query::QT_REAL);
        Py::Tuple result(7);
        Base::Vector3d v;

        v.x = mobox.Center[0];
        v.y = mobox.Center[1];
        v.z = mobox.Center[2];
        result.setItem(0, Py::Vector(v));

        v.x = mobox.Axis[0][0];
        v.y = mobox.Axis[0][1];
        v.z = mobox.Axis[0][2];
        result.setItem(1, Py::Vector(v));

        v.x = mobox.Axis[1][0];
        v.y = mobox.Axis[1][1];
        v.z = mobox.Axis[1][2];
        result.setItem(2, Py::Vector(v));

        v.x = mobox.Axis[2][0];
        v.y = mobox.Axis[2][1];
        v.z = mobox.Axis[2][2];
        result.setItem(3, Py::Vector(v));

        result.setItem(4, Py::Float(mobox.Extent[0]));
        result.setItem(5, Py::Float(mobox.Extent[1]));
        result.setItem(6, Py::Float(mobox.Extent[2]));

        return result;
    }
};

PyObject* initModule()
{
    return (new Module)->module().ptr();
}

} // namespace Mesh

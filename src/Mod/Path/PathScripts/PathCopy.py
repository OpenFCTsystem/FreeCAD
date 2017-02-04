# -*- coding: utf-8 -*-

# ***************************************************************************
# *                                                                         *
# *   Copyright (c) 2014 Yorik van Havre <yorik@uncreated.net>              *
# *                                                                         *
# *   This program is free software; you can redistribute it and/or modify  *
# *   it under the terms of the GNU Lesser General Public License (LGPL)    *
# *   as published by the Free Software Foundation; either version 2 of     *
# *   the License, or (at your option) any later version.                   *
# *   for detail see the LICENCE text file.                                 *
# *                                                                         *
# *   This program is distributed in the hope that it will be useful,       *
# *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
# *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
# *   GNU Library General Public License for more details.                  *
# *                                                                         *
# *   You should have received a copy of the GNU Library General Public     *
# *   License along with this program; if not, write to the Free Software   *
# *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  *
# *   USA                                                                   *
# *                                                                         *
# ***************************************************************************

import FreeCAD
import FreeCADGui
import Path
import math
from PathScripts.PathGeom import PathGeom
from PySide import QtCore, QtGui

"""Path Copy object and FreeCAD command"""

# Qt tanslation handling
try:
    _encoding = QtGui.QApplication.UnicodeUTF8

    def translate(context, text, disambig=None):
        return QtGui.QApplication.translate(context, text, disambig, _encoding)
except AttributeError:
    def translate(context, text, disambig=None):
        return QtGui.QApplication.translate(context, text, disambig)

class GCodeTransform:
    def __init__(self, rotation, pos):
        self.rotation = rotation
        self.xoffset = pos[0]
        self.yoffset = pos[1]
        self.zoffset = pos[2]

    def transform(self, path):
        CmdMoveRapid    = ['G0', 'G00']
        CmdMoveStraight = ['G1', 'G01']
        CmdMoveCW       = ['G2', 'G02']
        CmdMoveCCW      = ['G3', 'G03']
        CmdDrill        = ['G81', 'G82', 'G83']
        CmdMoveArc      = CmdMoveCW + CmdMoveCCW
        CmdMove         = CmdMoveStraight + CmdMoveArc

        commands = []
        ang = self.rotation/180*math.pi
        currX = 0
        currY = 0

        for cmd in path.Commands:
            if (cmd.Name in CmdMoveRapid) or (cmd.Name in CmdMove) or (cmd.Name in CmdDrill):
                params = cmd.Parameters
                x = params.get("X")
                if x is None:
                    x = currX
                currX = x
                y = params.get("Y")
                if y is None:
                    y = currY
                currY = y

                #rotation around origin:
                nx = x*math.cos(ang) - y*math.sin(ang)
                ny = y*math.cos(ang) + x*math.sin(ang)

                params.update({'X':nx+self.xoffset, 'Y': ny+self.yoffset})

                z = params.get("Z")
                if z is not None:
                    z += self.zoffset
                    params.update({"Z": z})

                #Arcs need to have the I and J params rotated as well
                if cmd.Name in CmdMoveArc:
                    i = params.get("I")
                    if i is None:
                        i = 0
                    j = params.get("J")
                    if j is None:
                        j = 0

                    ni = i*math.cos(ang) - j*math.sin(ang)
                    nj = j*math.cos(ang) + i*math.sin(ang)
                    params.update({'I':ni, 'J': nj})



                cmd.Parameters=params
            commands.append(cmd)
        newPath = Path.Path(commands)

        return newPath


class ObjectPathCopy:

    def __init__(self,obj):
        obj.addProperty("App::PropertyLink","Base","Path",QtCore.QT_TRANSLATE_NOOP("App::Property","The path to be copyed"))
        obj.addProperty("App::PropertyVectorDistance", "Position",
                        "Path", "Position of the Copy")
        obj.addProperty("App::PropertyAngle", "Rotation",
                        "Path", "Rotation angle of the copy")
        obj.Proxy = self

    def __getstate__(self):
        return None

    def __setstate__(self, state):
        return None

    def execute(self, obj):
        if obj.Base:
            if obj.Base.Path:
                basepath = obj.Base.Path
                trans = GCodeTransform(obj.Rotation, obj.Position)
                obj.Path = trans.transform(basepath)


class ViewProviderPathCopy:

    def __init__(self, vobj):
        vobj.Proxy = self

    def attach(self, vobj):
        self.Object = vobj.Object
        return

    def getIcon(self):
        return ":/icons/Path-Copy.svg"

    def __getstate__(self):
        return None

    def __setstate__(self, state):
        return None


class CommandPathCopy:

    def GetResources(self):
        return {'Pixmap': 'Path-Copy',
                'MenuText': QtCore.QT_TRANSLATE_NOOP("Path_Copy", "Copy"),
                'Accel': "P, Y",
                'ToolTip': QtCore.QT_TRANSLATE_NOOP("Path_Copy", "Creates a linked copy of another path")}

    def IsActive(self):
        if FreeCAD.ActiveDocument is not None:
            for o in FreeCAD.ActiveDocument.Objects:
                if o.Name[:3] == "Job":
                        return True
        return False

    def Activated(self):

        FreeCAD.ActiveDocument.openTransaction(
            translate("Path_Copy", "Create Copy"))
        FreeCADGui.addModule("PathScripts.PathCopy")

        consolecode = '''
import Path
import PathScripts
from PathScripts import PathCopy
selGood = True
# check that the selection contains exactly what we want
selection = FreeCADGui.Selection.getSelection()
proj = selection[0].InList[0] #get the group that the selectied object is inside

if len(selection) != 1:
    FreeCAD.Console.PrintError(translate("PathCopy","Please select one path object\\n"))
    selGood = False

if not selection[0].isDerivedFrom("Path::Feature"):
    FreeCAD.Console.PrintError(translate("PathCopy","The selected object is not a path\\n"))
    selGood = False

if selGood:
    obj = FreeCAD.ActiveDocument.addObject("Path::FeaturePython", str(selection[0].Name)+'_Copy')
    PathScripts.PathCopy.ObjectPathCopy(obj)
    PathScripts.PathCopy.ViewProviderPathCopy(obj.ViewObject)
    obj.Base = FreeCAD.ActiveDocument.getObject(selection[0].Name)

g = proj.Group
g.append(obj)
proj.Group = g

FreeCAD.ActiveDocument.recompute()

'''

        FreeCADGui.doCommand(consolecode)
        FreeCAD.ActiveDocument.commitTransaction()
        FreeCAD.ActiveDocument.recompute()


if FreeCAD.GuiUp:
    # register the FreeCAD command
    FreeCADGui.addCommand('Path_Copy', CommandPathCopy())

FreeCAD.Console.PrintLog("Loading PathCopy... done\n")

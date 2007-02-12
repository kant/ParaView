/*=========================================================================

   Program: ParaView
   Module:    pqFileDialogFavoriteModel.cxx

   Copyright (c) 2005,2006 Sandia Corporation, Kitware Inc.
   All rights reserved.

   ParaView is a free software; you can redistribute it and/or modify it
   under the terms of the ParaView license version 1.1. 

   See License_v1.1.txt for the full ParaView license.
   A copy of this license can be obtained by contacting
   Kitware Inc.
   28 Corporate Drive
   Clifton Park, NY 12065
   USA

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

=========================================================================*/

#include "pqFileDialogFavoriteModel.h"

#include <pqServer.h>
#include <QFileIconProvider>
#include <vtkClientServerStream.h>
#include <vtkCollection.h>
#include <vtkCollectionIterator.h>
#include <vtkProcessModule.h>
#include <vtkPVFileInformation.h>
#include <vtkPVFileInformationHelper.h>
#include <vtkSmartPointer.h>
#include <vtkSMIntVectorProperty.h>
#include <vtkSMProxy.h>
#include <vtkSMProxyManager.h>
#include <vtkSMStringVectorProperty.h>
#include <vtkStringList.h>

#include "pqSMAdaptor.h"

/////////////////////////////////////////////////////////////////////
// Icons

Q_GLOBAL_STATIC(QFileIconProvider, Icons);

//////////////////////////////////////////////////////////////////////
// FileInfo

class pqFileDialogFavoriteModelFileInfo
{
public:
  pqFileDialogFavoriteModelFileInfo()
  {
  }

  pqFileDialogFavoriteModelFileInfo(const QString& l, const QString& filepath, 
           const bool isdir, const bool isroot) :
    Label(l),
    FilePath(filepath),
    IsDir(isdir),
    IsRoot(isroot)
  {
  }

  const QString& label() const
  {
    return this->Label;
  }

  const QString& filePath() const 
  {
    return this->FilePath;
  }
  
  const bool isDir() const
  {
    return this->IsDir;
  }
  
  const bool isRoot() const
  {
    return this->IsRoot;
  }

private:
  QString Label;
  QString FilePath;
  bool IsDir;
  bool IsRoot;
};

//////////////////////////////////////////////////////////////////
// FavoriteModel

class pqFileDialogFavoriteModel::pqImplementation
{
public:
  QList<pqFileDialogFavoriteModelFileInfo> FavoriteList;


  pqImplementation(pqServer* server)
  {
  vtkPVFileInformation* information = vtkPVFileInformation::New();

  if(server)
    {
    vtkProcessModule* pm = vtkProcessModule::GetProcessModule();  
    vtkSMProxyManager* pxm = vtkSMProxyManager::GetProxyManager();

    vtkSMProxy* helper = pxm->NewProxy("misc","FileInformationHelper");
    helper->SetConnectionID(server->GetConnectionID());
    helper->SetServers(vtkProcessModule::DATA_SERVER_ROOT);
    pqSMAdaptor::setElementProperty(helper->GetProperty("SpecialDirectories"),
                                    true);
    helper->UpdateVTKObjects();

    pm->GatherInformation(server->GetConnectionID(),
      vtkProcessModule::DATA_SERVER, information, helper->GetID(0));

    helper->Delete();
    }
  else
    {
    vtkPVFileInformationHelper* helper = vtkPVFileInformationHelper::New();
    helper->SetSpecialDirectories(1);
    information->CopyFromObject(helper);
    helper->Delete();
    }
  
  vtkCollectionIterator* iter = information->GetContents()->NewIterator();
  for (iter->InitTraversal(); 
       !iter->IsDoneWithTraversal(); iter->GoToNextItem())
    {
    vtkPVFileInformation* cur_info = vtkPVFileInformation::SafeDownCast(
      iter->GetCurrentObject());
    if (!cur_info)
      {
      continue;
      }
    int type = cur_info->GetType();
    this->FavoriteList.push_back(pqFileDialogFavoriteModelFileInfo(
        cur_info->GetName(), cur_info->GetFullPath(),
        (type == vtkPVFileInformation::DIRECTORY || 
          type == vtkPVFileInformation::DRIVE),
        (type == vtkPVFileInformation::DRIVE)));
    }

  iter->Delete();
  information->Delete();
  }
};



pqFileDialogFavoriteModel::pqFileDialogFavoriteModel(pqServer* server,
                                                     QObject* p)
: base(p), Implementation(new pqImplementation(server))
{
}

pqFileDialogFavoriteModel::~pqFileDialogFavoriteModel()
{
  delete this->Implementation;
}

QString pqFileDialogFavoriteModel::filePath(const QModelIndex& Index) const
{
  if(Index.row() < this->Implementation->FavoriteList.size())
    {
    pqFileDialogFavoriteModelFileInfo& file = this->Implementation->FavoriteList[Index.row()];
    return file.filePath();
    }
  return QString();
}

bool pqFileDialogFavoriteModel::isDir(const QModelIndex& Index) const
{
  if(Index.row() >= this->Implementation->FavoriteList.size())
    return false;
    
  pqFileDialogFavoriteModelFileInfo& file = this->Implementation->FavoriteList[Index.row()];
  return file.isDir();
}

QVariant pqFileDialogFavoriteModel::data(const QModelIndex& idx, int role) const
{
  if(!idx.isValid())
    return QVariant();

  if(idx.row() >= this->Implementation->FavoriteList.size())
    return QVariant();

  const pqFileDialogFavoriteModelFileInfo& file = this->Implementation->FavoriteList[idx.row()];
  switch(role)
    {
    case Qt::DisplayRole:
      switch(idx.column())
        {
        case 0:
          return file.label();
        }
    case Qt::DecorationRole:
      switch(idx.column())
        {
        case 0:
          if(file.isRoot())
            {
            return Icons()->icon(QFileIconProvider::Drive);
            }
          if(file.isDir())
            {
            return Icons()->icon(QFileIconProvider::Folder);
            }
          return Icons()->icon(QFileIconProvider::File);
        }
    }
    
  return QVariant();
}
  
int pqFileDialogFavoriteModel::rowCount(const QModelIndex&) const
{
  return this->Implementation->FavoriteList.size();
}
  
QVariant pqFileDialogFavoriteModel::headerData(int section, 
                            Qt::Orientation , int role) const
{
  switch(role)
    {
    case Qt::DisplayRole:
      switch(section)
        {
        case 0:
          return tr("Favorites");
        }
    }
    
  return QVariant();
}
 

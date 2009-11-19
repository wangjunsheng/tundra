// For conditions of distribution and use, see copyright notice in license.txt

/**
 *  @file   AbstractInventoryDataModel.h
 *  @brief  Abstract inventory data model, pure virtual class. Inherit this class to create your own inventory
 *          data models to use in InventoryItemModel.
 */

#ifndef incl_InventoryModule_AbstractInventoryDataModel_h
#define incl_InventoryModule_AbstractInventoryDataModel_h

#include "AbstractInventoryItem.h"
#include "WorldStream.h"

#include <QObject>

// Few useful defines.
#define STD_TO_QSTR(p) QString(p.c_str())
#define QSTR_TO_UUID(p) RexTypes::RexUUID(p.toStdString())

namespace Inventory
{
    /// Pure virtual class.
    class AbstractInventoryDataModel : public QObject
    {
        Q_OBJECT

    public:
        /// Default constructor.
        AbstractInventoryDataModel() {}

        /// Destructor.
        virtual ~AbstractInventoryDataModel() {}

        /// Set World Stream.
        /// @param world_stream WorldStream pointer.
        virtual void SetWorldStream(const ProtocolUtilities::WorldStreamPtr world_stream) = 0;

        /// @return First folder by the requested name or null if the folder isn't found.
        virtual AbstractInventoryItem *GetFirstChildFolderByName(const QString &name) const = 0;

        /// @return First folder by the requested id or null if the folder isn't found.
        virtual AbstractInventoryItem *GetChildFolderById(const QString &searchId) const = 0;

        /// @return First item by the requested id or null if the item isn't found.
        virtual AbstractInventoryItem *GetChildAssetById(const QString &searchId) const = 0;

        /// @return Pointer to the requested item, or null if not found.
        virtual AbstractInventoryItem *GetChildById(const QString &searchId) const = 0;

        /// Returns folder by requested id, or creates a new one if the folder doesnt exist,
        /// or returns null if the parent folder is invalid.
        /// @param id ID.
        /// @param parent Parent folder.
        /// @return Pointer to the existing or just created folder.
        virtual AbstractInventoryItem *GetOrCreateNewFolder(const QString &id, AbstractInventoryItem &parentFolder,
            const QString &name = "New Folder", const bool &notify_server = true) = 0;

        /// Returns asset requested id, or creates a new one if the folder doesnt exist.
        /// @param inventory_id Inventory ID.
        /// @param asset_id Asset ID.
        /// @param parent Parent folder.
        /// @return Pointer to the existing or just created asset.
        virtual AbstractInventoryItem *GetOrCreateNewAsset(const QString &inventory_id, const QString &asset_id,
            AbstractInventoryItem &parentFolder, const QString &name) = 0;

        /// Request inventory descendents for spesific folder from the server.
        /// @param item Folder.
        virtual void FetchInventoryDescendents(AbstractInventoryItem *item) = 0;

        /// Notifies server about item move operation.
        /// @item Inventory item.
        virtual void NotifyServerAboutItemMove(AbstractInventoryItem *item) = 0;

        /// Notifies server about item move operation.
        /// @item Inventory item.
        virtual void NotifyServerAboutItemCopy(AbstractInventoryItem *item) = 0;

        /// Notifies server about item remove operation.
        /// @item Inventory item.
        virtual void NotifyServerAboutItemRemove(AbstractInventoryItem *item) = 0;

        /// Notifies server about item update operation(e.g. name changed).
        /// @item Inventory item.
        virtual void NotifyServerAboutItemUpdate(AbstractInventoryItem *item, const QString &old_name) = 0;

        /// Uploads file.
        /// @param filename Filename.
        /// @param parent_folder Destination folder in inventory for upload.
        virtual void UploadFile(const QString &filename, AbstractInventoryItem *parent_folder) = 0;

        /// Uploads multiple files.
        /// @param filenames List of filenames.
        /// @param parent_folder Destination folder in inventory for upload.
        virtual void UploadFiles(QStringList &filenames, AbstractInventoryItem *parent_folder) = 0;

        /// Uploads multiple files, uses data buffers.
        /// @param filenames List of filenames.
        /// @param parent_folder Destination folder in inventory for upload.
        virtual void UploadFilesFromBuffer(QStringList &filenames, QVector<QVector<uchar> > &buffers,
            AbstractInventoryItem *parent_folder) = 0;

        /// Downloads asset/file from inventory.
        /// @param store_folder Store folder.
        /// @param selected_item Selected item in inventory.
        virtual void DownloadFile(const QString &store_folder, AbstractInventoryItem *selected_item) = 0;

        /// @return Inventory root folder.
        virtual AbstractInventoryItem *GetRoot() const = 0;

        /// @return Inventory trash folder.
        virtual AbstractInventoryItem *GetTrashFolder() const = 0;

    private:
        Q_DISABLE_COPY(AbstractInventoryDataModel);

    };
}
#endif

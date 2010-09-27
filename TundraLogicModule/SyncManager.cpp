// For conditions of distribution and use, see copyright notice in license.txt

#include "StableHeaders.h"
#include "ComponentManager.h"
#include "CoreStringUtils.h"
#include "DebugOperatorNew.h"
#include "KristalliProtocolModule.h"
#include "KristalliProtocolModuleEvents.h"
#include "SyncManager.h"
#include "SceneManager.h"
#include "Entity.h"
#include "TundraLogicModule.h"
#include "TundraMessages.h"
#include "MsgCreateEntity.h"
#include "MsgCreateComponents.h"
#include "MsgUpdateComponents.h"
#include "MsgRemoveComponents.h"
#include "MsgRemoveEntity.h"
#include "MsgEntityIDCollision.h"

#include <cstring>

#include "MemoryLeakCheck.h"

namespace TundraLogic
{

SyncManager::SyncManager(TundraLogicModule* owner, Foundation::Framework* fw) :
    owner_(owner),
    framework_(fw),
    update_period_(0.04),
    update_acc_(0.0)
{
}

SyncManager::~SyncManager()
{
}

void SyncManager::SetUpdatePeriod(f64 period)
{
    // Allow max 100fps
    if (period < 0.01)
        period = 0.01;
    update_period_ = period;
}

void SyncManager::RegisterToScene(Scene::ScenePtr scene)
{
    // Disconnect from previous scene if not expired
    Scene::ScenePtr previous = scene_.lock();
    if (previous)
    {
        disconnect(this);
        server_syncstate_.Clear();
    }
    
    scene_.reset();
    
    if (!scene)
    {
        TundraLogicModule::LogError("Null scene, cannot replicate");
        return;
    }
    
    scene_ = scene;
    Scene::SceneManager* sceneptr = scene.get();
    
    connect(sceneptr, SIGNAL( ComponentChanged(IComponent*, AttributeChange::Type) ),
        this, SLOT( OnComponentChanged(IComponent*, AttributeChange::Type) ));
    connect(sceneptr, SIGNAL( ComponentAdded(Scene::Entity*, IComponent*, AttributeChange::Type) ),
        this, SLOT( OnComponentAdded(Scene::Entity*, IComponent*, AttributeChange::Type) ));
    connect(sceneptr, SIGNAL( ComponentRemoved(Scene::Entity*, IComponent*, AttributeChange::Type) ),
        this, SLOT( OnComponentRemoved(Scene::Entity*, IComponent*, AttributeChange::Type) ));
    connect(sceneptr, SIGNAL( EntityCreated(Scene::Entity*, AttributeChange::Type) ),
        this, SLOT( OnEntityCreated(Scene::Entity*, AttributeChange::Type) ));
    connect(sceneptr, SIGNAL( EntityRemoved(Scene::Entity*, AttributeChange::Type) ),
        this, SLOT( OnEntityRemoved(Scene::Entity*, AttributeChange::Type) ));
}

void SyncManager::HandleKristalliEvent(event_id_t event_id, IEventData* data)
{
    if (event_id == KristalliProtocol::Events::NETMESSAGE_IN)
    {
        KristalliProtocol::Events::KristalliNetMessageIn* eventData = checked_static_cast<KristalliProtocol::Events::KristalliNetMessageIn*>(data);
        HandleKristalliMessage(eventData->source, eventData->id, eventData->data, eventData->numBytes);
    }
}

void SyncManager::HandleKristalliMessage(MessageConnection* source, message_id_t id, const char* data, size_t numBytes)
{
    switch (id)
    {
    case cCreateEntityMessage:
        {
            MsgCreateEntity msg(data, numBytes);
            HandleCreateEntity(source, msg);
        }
        break;
    case cRemoveEntityMessage:
        {
            MsgRemoveEntity msg(data, numBytes);
            HandleRemoveEntity(source, msg);
        }
        break;
    case cCreateComponentsMessage:
        {
            MsgCreateComponents msg(data, numBytes);
            HandleCreateComponents(source, msg);
        }
        break;
    case cUpdateComponentsMessage:
        {
            MsgUpdateComponents msg(data, numBytes);
            HandleUpdateComponents(source, msg);
        }
        break;
    case cRemoveComponentsMessage:
        {
            MsgRemoveComponents msg(data, numBytes);
            HandleRemoveComponents(source, msg);
        }
        break;
    case cEntityIDCollisionMessage:
        {
            MsgEntityIDCollision msg(data, numBytes);
            HandleEntityIDCollision(source, msg);
        }
        break;
    }
}

void SyncManager::NewUserConnected(KristalliProtocol::UserConnection* user)
{
    PROFILE(SyncManager_NewUserConnected);

    Scene::ScenePtr scene = scene_.lock();
    if (!scene)
        return;
    
    // If user does not have replication state, create it, then mark all non-local entities dirty
    // so we will send them during the coming updates
    if (!user->userData)
        user->userData = boost::shared_ptr<IUserData>(new SceneSyncState());
    
    SceneSyncState* state = checked_static_cast<SceneSyncState*>(user->userData.get());
    
    for(Scene::SceneManager::iterator iter = scene->begin(); iter != scene->end(); ++iter)
    {
        Scene::EntityPtr entity = *iter;
        entity_id_t id = entity->GetId();
        // If we cross over to local entities (ID range 0x80000000 - 0xffffffff), break
        if (id & Scene::LocalEntity)
            break;
        state->OnEntityChanged(id);
    }
}

void SyncManager::OnComponentChanged(IComponent* comp, AttributeChange::Type change)
{
    if (!comp->IsSerializable())
        return;
    if ((change != AttributeChange::Local) || (!comp->GetNetworkSyncEnabled()))
        return;
    Scene::Entity* entity = comp->GetParentEntity();
    if ((!entity) || (entity->IsLocal()))
        return;
    
    if (owner_->IsServer())
    {
        KristalliProtocol::UserConnectionList& users = owner_->GetKristalliModule()->GetUserConnections();
        for (KristalliProtocol::UserConnectionList::iterator i = users.begin(); i != users.end(); ++i)
        {
            SceneSyncState* state = checked_static_cast<SceneSyncState*>(i->userData.get());
            if (state)
                state->OnComponentChanged(entity->GetId(), comp->TypeNameHash(), comp->Name());
        }
    }
    else
    {
        SceneSyncState* state = &server_syncstate_;
        state->OnComponentChanged(entity->GetId(), comp->TypeNameHash(), comp->Name());
    }
}

void SyncManager::OnComponentAdded(Scene::Entity* entity, IComponent* comp, AttributeChange::Type change)
{
    if (!comp->IsSerializable())
        return;
    if ((change != AttributeChange::Local) || (!comp->GetNetworkSyncEnabled()))
        return;
    if (entity->IsLocal())
        return;
    
    if (owner_->IsServer())
    {
        KristalliProtocol::UserConnectionList& users = owner_->GetKristalliModule()->GetUserConnections();
        for (KristalliProtocol::UserConnectionList::iterator i = users.begin(); i != users.end(); ++i)
        {
            SceneSyncState* state = checked_static_cast<SceneSyncState*>(i->userData.get());
            if (state)
                state->OnComponentChanged(entity->GetId(), comp->TypeNameHash(), comp->Name());
        }
    }
    else
    {
        SceneSyncState* state = &server_syncstate_;
        state->OnComponentChanged(entity->GetId(), comp->TypeNameHash(), comp->Name());
    }
}

void SyncManager::OnComponentRemoved(Scene::Entity* entity, IComponent* comp, AttributeChange::Type change)
{
    if (!comp->IsSerializable())
        return;
    if ((change != AttributeChange::Local) || (!comp->GetNetworkSyncEnabled()))
        return;
    if (entity->IsLocal())
        return;
    
    if (owner_->IsServer())
    {
        KristalliProtocol::UserConnectionList& users = owner_->GetKristalliModule()->GetUserConnections();
        for (KristalliProtocol::UserConnectionList::iterator i = users.begin(); i != users.end(); ++i)
        {
            SceneSyncState* state = checked_static_cast<SceneSyncState*>(i->userData.get());
            if (state)
                state->OnComponentRemoved(entity->GetId(), comp->TypeNameHash(), comp->Name());
        }
    }
    else
    {
        SceneSyncState* state = &server_syncstate_;
        state->OnComponentRemoved(entity->GetId(), comp->TypeNameHash(), comp->Name());
    }
}

void SyncManager::OnEntityCreated(Scene::Entity* entity, AttributeChange::Type change)
{
    if ((change != AttributeChange::Local) || (entity->IsLocal()))
        return;
    
    if (owner_->IsServer())
    {
        KristalliProtocol::UserConnectionList& users = owner_->GetKristalliModule()->GetUserConnections();
        for (KristalliProtocol::UserConnectionList::iterator i = users.begin(); i != users.end(); ++i)
        {
            SceneSyncState* state = checked_static_cast<SceneSyncState*>(i->userData.get());
            if (state)
                state->OnEntityChanged(entity->GetId());
        }
    }
    else
    {
        SceneSyncState* state = &server_syncstate_;
        state->OnEntityChanged(entity->GetId());
    }
}

void SyncManager::OnEntityRemoved(Scene::Entity* entity, AttributeChange::Type change)
{
    if (change != AttributeChange::Local)
        return;
    if (entity->IsLocal())
        return;
    
    if (owner_->IsServer())
    {
        KristalliProtocol::UserConnectionList& users = owner_->GetKristalliModule()->GetUserConnections();
        for (KristalliProtocol::UserConnectionList::iterator i = users.begin(); i != users.end(); ++i)
        {
            SceneSyncState* state = checked_static_cast<SceneSyncState*>(i->userData.get());
            if (state)
                state->OnEntityRemoved(entity->GetId());
        }
    }
    else
    {
        SceneSyncState* state = &server_syncstate_;
        state->OnEntityRemoved(entity->GetId());
    }
}

void SyncManager::Update(f64 frametime)
{
    PROFILE(SyncManager_Update);
    
    update_acc_ += frametime;
    if (update_acc_ < update_period_)
        return;
    // If multiple updates passed, update still just once
    while (update_acc_ >= update_period_)
        update_acc_ -= update_period_;
    
    Scene::ScenePtr scene = scene_.lock();
    if (!scene)
        return;
    
    if (owner_->IsServer())
    {
        // If we are server, process all users
        KristalliProtocol::UserConnectionList& users = owner_->GetKristalliModule()->GetUserConnections();
        for (KristalliProtocol::UserConnectionList::iterator i = users.begin(); i != users.end(); ++i)
        {
            SceneSyncState* state = checked_static_cast<SceneSyncState*>(i->userData.get());
            if (state)
                ProcessSyncState(i->connection, state);
        }
    }
    else
    {
        // If we are client, process just the server sync state
        MessageConnection* connection = owner_->GetKristalliModule()->GetMessageConnection();
        if (connection)
            ProcessSyncState(connection, &server_syncstate_);
    }
}

void SyncManager::ProcessSyncState(MessageConnection* destination, SceneSyncState* state)
{
    PROFILE(SyncManager_ProcessSyncState);
    
    Scene::ScenePtr scene = scene_.lock();
    
    int num_messages_sent = 0;
    
    //! \todo Always sends everything that is dirty/removed. No priorization or limiting of sent data size yet.
    
    // Process dirty entities (added/updated/removed components)
    std::set<entity_id_t> dirty = state->dirty_entities_;
    for (std::set<entity_id_t>::iterator i = dirty.begin(); i != dirty.end(); ++i)
    {
        Scene::EntityPtr entity = scene->GetEntity(*i);
        if (!entity)
            continue;
        const Scene::Entity::ComponentVector &components = entity->GetComponentVector();
        EntitySyncState* entitystate = state->GetEntity(*i);
        // No record in entitystate -> newly created entity, send full state
        if (!entitystate)
        {
            entitystate = state->GetOrCreateEntity(*i);
            MsgCreateEntity msg;
            msg.entityID = entity->GetId();
            for(uint j = 0; j < components.size(); ++j)
            {
                IComponent* component = components[j].get();
                
                if ((component->IsSerializable()) && (component->GetNetworkSyncEnabled()))
                {
                    // Create componentstate, then fill the initial data both there and to network stream
                    ComponentSyncState* componentstate = entitystate->GetOrCreateComponent(component->TypeNameHash(), component->Name());
                    
                    MsgCreateEntity::S_components newComponent;
                    newComponent.componentTypeHash = component->TypeNameHash();
                    newComponent.componentName = StringToBuffer(component->Name().toStdString());
                    newComponent.componentData.resize(64 * 1024);
                    DataSerializer dest((char*)&newComponent.componentData[0], newComponent.componentData.size());
                    component->SerializeToBinary(dest);
                    newComponent.componentData.resize(dest.BytesFilled());
                    // Copy to state if data size nonzero
                    if (newComponent.componentData.size())
                    {
                        componentstate->data_.resize(newComponent.componentData.size());
                        memcpy(&componentstate->data_[0], &newComponent.componentData[0], newComponent.componentData.size());
                        msg.components.push_back(newComponent);
                    }
                    
                    destination->Send(msg);
                    ++num_messages_sent;
                }
                
                entitystate->AckDirty(component->TypeNameHash(), component->Name());
            }
        }
        else
        {
            // Existing entitystate, check created & modified components
            //! \todo Renaming an existing component, that already has been replicated to client, leads to duplication.
            //! So it's not currently supported sensibly.
            {
                std::set<std::pair<uint, QString> > dirtycomps = entitystate->dirty_components_;
                MsgCreateComponents createMsg;
                createMsg.entityID = entity->GetId();
                MsgUpdateComponents updateMsg;
                updateMsg.entityID = entity->GetId();
                
                for (std::set<std::pair<uint, QString> >::iterator j = dirtycomps.begin(); j != dirtycomps.end(); ++j)
                {
                    IComponent* component = entity->GetComponent(j->first, j->second).get();
                    if ((component) && (component->IsSerializable()) && (component->GetNetworkSyncEnabled()))
                    {
                        ComponentSyncState* componentstate = entitystate->GetComponent(component->TypeNameHash(), component->Name());
                        // New component or zero-size previous data
                        if ((!componentstate) || (componentstate->data_.size() == 0))
                        {
                            componentstate = entitystate->GetOrCreateComponent(component->TypeNameHash(), component->Name());
                            MsgCreateComponents::S_components newComponent;
                            newComponent.componentTypeHash = component->TypeNameHash();
                            newComponent.componentName = StringToBuffer(component->Name().toStdString());
                            newComponent.componentData.resize(64 * 1024);
                            DataSerializer dest((char*)&newComponent.componentData[0], newComponent.componentData.size());
                            component->SerializeToBinary(dest);
                            newComponent.componentData.resize(dest.BytesFilled());
                            // Copy to state if data size nonzero
                            if (newComponent.componentData.size())
                            {
                                componentstate->data_.resize(newComponent.componentData.size());
                                memcpy(&componentstate->data_[0], &newComponent.componentData[0], newComponent.componentData.size());
                                createMsg.components.push_back(newComponent);
                            }
                        }
                        else
                        {
                            // Existing data, compare and deltaserialize
                            MsgUpdateComponents::S_components updComponent;
                            updComponent.componentTypeHash = component->TypeNameHash();
                            updComponent.componentName = StringToBuffer(component->Name().toStdString());
                            updComponent.componentData.resize(64 * 1024);
                            DataSerializer dest((char*)&updComponent.componentData[0], updComponent.componentData.size());
                            DataDeserializer prev((char*)&componentstate->data_[0], componentstate->data_.size());
                            // Copy to state & add to message if there was something to write
                            if ((component->SerializeToDeltaBinary(dest, prev)) && (dest.BytesFilled()))
                            {
                                updComponent.componentData.resize(dest.BytesFilled());
                                memcpy(&componentstate->data_[0], &updComponent.componentData[0], updComponent.componentData.size());
                                updateMsg.components.push_back(updComponent);
                            }
                        }
                    }
                    entitystate->AckDirty(j->first, j->second);
                }
                
                // Send message(s) only if there were components
                if (createMsg.components.size())
                {
                    destination->Send(createMsg);
                    ++num_messages_sent;
                }
                if (updateMsg.components.size())
                {
                    destination->Send(updateMsg);
                    ++num_messages_sent;
                }
            }
            
            // Check removed components
            {
                std::set<std::pair<uint, QString> > removedcomps = entitystate->removed_components_;
                MsgRemoveComponents removeMsg;
                removeMsg.entityID = entity->GetId();
                
                for (std::set<std::pair<uint, QString> >::iterator j = removedcomps.begin(); j != removedcomps.end(); ++j)
                {
                    MsgRemoveComponents::S_components remComponent;
                    remComponent.componentTypeHash = j->first;
                    remComponent.componentName = StringToBuffer(j->second.toStdString());
                    removeMsg.components.push_back(remComponent);
                    
                    entitystate->RemoveComponent(j->first, j->second);
                    entitystate->AckRemove(j->first, j->second);
                }
            }
        }
        
        state->AckDirty(*i);
        
    }

    // Process removed entities
    std::set<entity_id_t> removed = state->removed_entities_;
    for (std::set<entity_id_t>::iterator i = removed.begin(); i != removed.end(); ++i)
    {
        MsgRemoveEntity msg;
        msg.entityID = *i;
        destination->Send(msg);
        state->RemoveEntity(*i);
        state->AckRemove(*i);
        ++num_messages_sent;
        
    }
    
    if (num_messages_sent)
        TundraLogicModule::LogDebug("Sent " + ToString<int>(num_messages_sent) + " scenesync messages");
}

bool SyncManager::ValidateAction(MessageConnection* source, unsigned messageID, entity_id_t entityID)
{
    if (entityID & Scene::LocalEntity)
    {
        TundraLogicModule::LogWarning("Received an entity sync message for a local entity. Disregarding.");
        return false;
    }
    
    // For now, always trust scene actions from server
    if (!owner_->IsServer())
        return true;
    
    // And for now, always also trust scene actions from clients, if they are known and authenticated
    KristalliProtocol::UserConnection* user = owner_->GetKristalliModule()->GetUserConnection(source);
    if ((!user) || (!user->authenticated))
        return false;
    
    return true;
}

void SyncManager::HandleCreateEntity(MessageConnection* source, const MsgCreateEntity& msg)
{
    Scene::ScenePtr scene = framework_->GetDefaultWorldScene();
    if (!scene)
        return;
    
    entity_id_t entityID = msg.entityID;
    if (!ValidateAction(source, msg.MessageID(), entityID))
        return;
    
    // Get matching syncstate for reflecting the changes
    SceneSyncState* state = GetSceneSyncState(source);
    if (!state)
    {
        TundraLogicModule::LogWarning("Null syncstate for connection! Disregarding CreateEntity message");
        return;
    }
    
    bool isServer = owner_->IsServer();
    
    // For clients, the change type is Network. For server, the change type is Local, so that it will get replicated to all clients in turn
    AttributeChange::Type change = isServer ? AttributeChange::Local : AttributeChange::Network;
    
    // Check for ID collision
    if (isServer)
    {
        if (scene->GetEntity(entityID))
        {
            entity_id_t newEntityID = scene->GetNextFreeId();
            // Send information to the creator that the ID was taken. The reserved ID will never get replicated to others
            MsgEntityIDCollision collisionMsg;
            collisionMsg.oldEntityID = entityID;
            collisionMsg.newEntityID = newEntityID;
            source->Send(collisionMsg);
            entityID = newEntityID;
        }
    }
    else
    {
        // If a client gets a entity that already exists, destroy it forcibly
        if (scene->GetEntity(entityID))
        {
            TundraLogicModule::LogDebug("Received entity creation from server for entity ID " + ToString<int>(entityID) + " that already exists. Removing the old entity.");
            scene->RemoveEntity(entityID, change);
        }
    }
    
    Scene::EntityPtr entity = scene->CreateEntity(entityID);
    if (!entity)
    {
        TundraLogicModule::LogWarning("Scene refused to create entity " + ToString<int>(entityID));
        return;
    }
    
    // Reflect changes back to syncstate
    state->GetOrCreateEntity(entityID);
    
    // Read the components
    for (uint i = 0; i < msg.components.size(); ++i)
    {
        uint type_hash = msg.components[i].componentTypeHash;
        QString name = QString::fromStdString(BufferToString(msg.components[i].componentName));
        ComponentPtr new_comp = entity->GetOrCreateComponent(type_hash, name);
        if (new_comp)
        {
            if (msg.components[i].componentData.size())
            {
                DataDeserializer source((const char*)&msg.components[i].componentData[0], msg.components[i].componentData.size());
                try
                {
                    new_comp->DeserializeFromBinary(source, change);
                }
                catch (...)
                {
                    TundraLogicModule::LogError("Error while deserializing component " + framework_->GetComponentManager()->GetComponentTypeName(type_hash).toStdString());
                }
                
                // Reflect changes back to syncstate
                EntitySyncState* entitystate = state->GetOrCreateEntity(entityID);
                ComponentSyncState* componentstate = entitystate->GetOrCreateComponent(type_hash, name);
                componentstate->data_.resize(msg.components[i].componentData.size());
                memcpy(&componentstate->data_[0], &msg.components[i].componentData[0], msg.components[i].componentData.size());
            }
        }
        else
            TundraLogicModule::LogWarning("Could not create component " + framework_->GetComponentManager()->GetComponentTypeName(type_hash).toStdString());
    }
    
    // Then emit the entity/componentchanges
    scene->EmitEntityCreated(entity, change);
    // Kind of a "hack", call OnChanged to the components only after all components have been loaded
    // This allows to resolve component references to the same entity (for example to the Placeable) at this point
    const Scene::Entity::ComponentVector &components = entity->GetComponentVector();
    for(uint i = 0; i < components.size(); ++i)
        components[i]->ComponentChanged(change);
    // Finally, if changetype is Network, reset it so that it won't show "dirty" status which may be confusing
    if (change == AttributeChange::Network)
        entity->ResetChange();
}

void SyncManager::HandleRemoveEntity(MessageConnection* source, const MsgRemoveEntity& msg)
{
    Scene::ScenePtr scene = framework_->GetDefaultWorldScene();
    if (!scene)
        return;
    
    entity_id_t entityID = msg.entityID;
    if (!ValidateAction(source, msg.MessageID(), entityID))
        return;
    
    // Get matching syncstate for reflecting the changes
    SceneSyncState* state = GetSceneSyncState(source);
    if (!state)
    {
        TundraLogicModule::LogWarning("Null syncstate for connection! Disregarding RemoveEntity message");
        return;
    }
    
    bool isServer = owner_->IsServer();
    
    // For clients, the change type is Network. For server, the change type is Local, so that it will get replicated to all clients in turn
    AttributeChange::Type change = isServer ? AttributeChange::Local : AttributeChange::Network;
    
    scene->RemoveEntity(entityID, change);
    
    // Reflect changes back to syncstate
    state->RemoveEntity(entityID);
}


void SyncManager::HandleCreateComponents(MessageConnection* source, const MsgCreateComponents& msg)
{
    Scene::ScenePtr scene = framework_->GetDefaultWorldScene();
    if (!scene)
        return;
    
    entity_id_t entityID = msg.entityID;
    if (!ValidateAction(source, msg.MessageID(), entityID))
        return;
    
    // Get matching syncstate for reflecting the changes
    SceneSyncState* state = GetSceneSyncState(source);
    if (!state)
    {
        TundraLogicModule::LogWarning("Null syncstate for connection! Disregarding CreateComponents message");
        return;
    }
    
    bool isServer = owner_->IsServer();
    
    // For clients, the change type is Network. For server, the change type is Local, so that it will get replicated to all clients in turn
    AttributeChange::Type change = isServer ? AttributeChange::Local : AttributeChange::Network;
    
    // See if we can find the entity. If not, create it, should not happen, but we handle it anyway (!!!)
    Scene::EntityPtr entity = scene->GetEntity(entityID);
    if (!entity)
    {
        TundraLogicModule::LogWarning("Entity " + ToString<int>(entityID) + " not found for CreateComponents message, creating it now");
        entity = scene->CreateEntity(entityID);
        if (!entity)
        {
            TundraLogicModule::LogWarning("Scene refused to create entity " + ToString<int>(entityID));
            return;
        }
        
        // Reflect changes back to syncstate
        state->GetOrCreateEntity(entityID);
    }
    
    // Read the components. These are not deltaserialized.
    std::vector<ComponentPtr> actually_changed_components;
    for (uint i = 0; i < msg.components.size(); ++i)
    {
        uint type_hash = msg.components[i].componentTypeHash;
        QString name = QString::fromStdString(BufferToString(msg.components[i].componentName));
        ComponentPtr new_comp = entity->GetOrCreateComponent(type_hash, name);
        if (new_comp)
        {
            if (msg.components[i].componentData.size())
            {
                DataDeserializer source((const char*)&msg.components[i].componentData[0], msg.components[i].componentData.size());
                
                try
                {
                    new_comp->DeserializeFromBinary(source, change);
                    actually_changed_components.push_back(new_comp);
                }
                catch (...)
                {
                    TundraLogicModule::LogError("Error while deserializing component " + framework_->GetComponentManager()->GetComponentTypeName(type_hash).toStdString());
                }
                
                // Reflect changes back to syncstate
                EntitySyncState* entitystate = state->GetOrCreateEntity(entityID);
                ComponentSyncState* componentstate = entitystate->GetOrCreateComponent(type_hash, name);
                componentstate->data_.resize(msg.components[i].componentData.size());
                memcpy(&componentstate->data_[0], &msg.components[i].componentData[0], msg.components[i].componentData.size());
            }
        }
        else
            TundraLogicModule::LogWarning("Could not create component " + framework_->GetComponentManager()->GetComponentTypeName(type_hash).toStdString());
    }
    
    // Then emit the componentchanges
    // Kind of a "hack", call OnChanged to the components only after all components have been created, to prevent reacting to possibly incoherent state
    if (actually_changed_components.size())
    {
        for(uint i = 0; i < actually_changed_components.size(); ++i)
            actually_changed_components[i]->ComponentChanged(change);
        // Finally, if changetype is Network, reset it so that it won't show "dirty" status which may be confusing
        if (change == AttributeChange::Network)
            entity->ResetChange();
    }
}

void SyncManager::HandleUpdateComponents(MessageConnection* source, const MsgUpdateComponents& msg)
{
    Scene::ScenePtr scene = framework_->GetDefaultWorldScene();
    if (!scene)
        return;
    
    entity_id_t entityID = msg.entityID;
    if (!ValidateAction(source, msg.MessageID(), entityID))
        return;
    
    // Get matching syncstate for reflecting the changes
    SceneSyncState* state = GetSceneSyncState(source);
    if (!state)
    {
        TundraLogicModule::LogWarning("Null syncstate for connection! Disregarding UpdateComponents message");
        return;
    }
    
    bool isServer = owner_->IsServer();
    
    // For clients, the change type is Network. For server, the change type is Local, so that it will get replicated to all clients in turn
    AttributeChange::Type change = isServer ? AttributeChange::Local : AttributeChange::Network;
    
    // See if we can find the entity. If not, create it, should not happen, but we handle it anyway (!!!)
    Scene::EntityPtr entity = scene->GetEntity(entityID);
    if (!entity)
    {
        TundraLogicModule::LogWarning("Entity " + ToString<int>(entityID) + " not found for UpdateComponents message, creating it now");
        entity = scene->CreateEntity(entityID);
        if (!entity)
        {
            TundraLogicModule::LogWarning("Scene refused to create entity " + ToString<int>(entityID));
            return;
        }
        
        // Reflect changes back to syncstate
        state->GetOrCreateEntity(entityID);
    }
    
    // Read the components
    std::vector<ComponentPtr> actually_changed_components;
    for (uint i = 0; i < msg.components.size(); ++i)
    {
        uint type_hash = msg.components[i].componentTypeHash;
        QString name = QString::fromStdString(BufferToString(msg.components[i].componentName));
        ComponentPtr new_comp = entity->GetOrCreateComponent(type_hash, name);
        if (new_comp)
        {
            if (msg.components[i].componentData.size())
            {
                DataDeserializer source((const char*)&msg.components[i].componentData[0], msg.components[i].componentData.size());
                try
                {
                    if (new_comp->DeserializeFromDeltaBinary(source, change))
                        actually_changed_components.push_back(new_comp);
                }
                catch (...)
                {
                    TundraLogicModule::LogError("Error while delta-deserializing component " + framework_->GetComponentManager()->GetComponentTypeName(type_hash).toStdString());
                }
                
                // Reflect changes back to syncstate
                // For this, we need to do a kind of ugly thing: re-serialize the attributes to a fullstate buffer
                EntitySyncState* entitystate = state->GetOrCreateEntity(entityID);
                ComponentSyncState* componentstate = entitystate->GetOrCreateComponent(type_hash, name);
                componentstate->data_.resize(64 * 1024);
                DataSerializer dest((char*)&componentstate->data_[0], componentstate->data_.size());
                new_comp->SerializeToBinary(dest);
                componentstate->data_.resize(dest.BytesFilled());
            }
        }
        else
            TundraLogicModule::LogWarning("Could not create component " + framework_->GetComponentManager()->GetComponentTypeName(type_hash).toStdString());
    }
    
    // Then emit the componentchanges
    // Kind of a "hack", call OnChanged to the components only after all components have been updated, to prevent reacting to possibly incoherent state
    if (actually_changed_components.size())
    {
        for(uint i = 0; i < actually_changed_components.size(); ++i)
            actually_changed_components[i]->ComponentChanged(change);
        // Finally, if changetype is Network, reset it so that it won't show "dirty" status which may be confusing
        if (change == AttributeChange::Network)
            entity->ResetChange();
    }
}

void SyncManager::HandleRemoveComponents(MessageConnection* source, const MsgRemoveComponents& msg)
{
    Scene::ScenePtr scene = framework_->GetDefaultWorldScene();
    if (!scene)
        return;
    
    entity_id_t entityID = msg.entityID;
    if (!ValidateAction(source, msg.MessageID(), entityID))
        return;
    
    // Get matching syncstate for reflecting the changes
    SceneSyncState* state = GetSceneSyncState(source);
    if (!state)
    {
        TundraLogicModule::LogWarning("Null syncstate for connection! Disregarding RemoveComponents message");
        return;
    }
    
    bool isServer = owner_->IsServer();
    
    // For clients, the change type is Network. For server, the change type is Local, so that it will get replicated to all clients in turn
    AttributeChange::Type change = isServer ? AttributeChange::Local : AttributeChange::Network;
    
    Scene::EntityPtr entity = scene->GetEntity(msg.entityID);
    if (!entity)
        return;
    
    for (unsigned i = 0; i < msg.components.size(); ++i)
    {
        uint type_hash = msg.components[i].componentTypeHash;
        QString name = QString::fromStdString(BufferToString(msg.components[i].componentName));
        
        ComponentPtr comp = entity->GetComponent(type_hash, name);
        if (comp)
        {
            entity->RemoveComponent(comp, change);
            comp.reset();
        }
        
        // Reflect changes back to syncstate
        EntitySyncState* entitystate = state->GetEntity(entityID);
        if (entitystate)
            entitystate->RemoveComponent(type_hash, name);
    }
}

void SyncManager::HandleEntityIDCollision(MessageConnection* source, const MsgEntityIDCollision& msg)
{
    Scene::ScenePtr scene = framework_->GetDefaultWorldScene();
    if (!scene)
        return;
    
    if (owner_->IsServer())
    {
        TundraLogicModule::LogWarning("Received EntityIDCollision from a client, disregarding.");
        return;
    }
    
    TundraLogicModule::LogDebug("An entity ID collision occurred. Entity " + ToString<int>(msg.oldEntityID) + " became " + ToString<int>(msg.newEntityID));
    scene->ChangeEntityId(msg.oldEntityID, msg.newEntityID);
    
    // Do the change also in server scene replication state
    SceneSyncState* state = GetSceneSyncState(source);
    if (state)
    {
        state->entities_[msg.newEntityID] = state->entities_[msg.oldEntityID];
        state->RemoveEntity(msg.oldEntityID);
    }
}

SceneSyncState* SyncManager::GetSceneSyncState(MessageConnection* connection)
{
    if (!owner_->IsServer())
        return &server_syncstate_;
    
    KristalliProtocol::UserConnectionList& users = owner_->GetKristalliModule()->GetUserConnections();
    for (KristalliProtocol::UserConnectionList::iterator i = users.begin(); i != users.end(); ++i)
    {
        if (i->connection == connection)
            return checked_static_cast<SceneSyncState*>(i->userData.get());
    }
    return 0;
}

}

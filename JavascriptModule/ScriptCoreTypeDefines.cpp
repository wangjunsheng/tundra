// For conditions of distribution and use, see copyright notice in license.txt

#include "StableHeaders.h"
#include "DebugOperatorNew.h"
#include "Color.h"
#include "Quaternion.h"
#include "Transform.h"
#include "Vector3D.h"
#include "Matrix4.h"
#include "IAttribute.h"
#include "AssetReference.h"
#include "Entity.h"
#include "ScriptMetaTypeDefines.h"
#include "SceneManager.h"
#include "LoggingFunctions.h"

#include <QScriptEngine>
#include <QColor>
#include <QVector3D>
#include <QQuaternion>
#include <QScriptValueIterator>

#include "MemoryLeakCheck.h"

Q_DECLARE_METATYPE(IAttribute*);
Q_DECLARE_METATYPE(ScenePtr);
Q_DECLARE_METATYPE(EntityPtr);
Q_DECLARE_METATYPE(ComponentPtr);
Q_DECLARE_METATYPE(QList<Entity*>);
Q_DECLARE_METATYPE(Entity*);
Q_DECLARE_METATYPE(std::string);
Q_DECLARE_METATYPE(EntityList);

QScriptValue toScriptValueColor(QScriptEngine *engine, const Color &s)
{
    QScriptValue obj = engine->newObject();
    obj.setProperty("r", QScriptValue(engine, s.r));
    obj.setProperty("g", QScriptValue(engine, s.g));
    obj.setProperty("b", QScriptValue(engine, s.b));
    obj.setProperty("a", QScriptValue(engine, s.a));
    return obj;
}

void fromScriptValueColor(const QScriptValue &obj, Color &s)
{
    s.r = (float)obj.property("r").toNumber();
    s.g = (float)obj.property("g").toNumber();
    s.b = (float)obj.property("b").toNumber();
    s.a = (float)obj.property("a").toNumber();
}

QScriptValue Vector3df_prototype_normalize(QScriptContext *ctx, QScriptEngine *engine);
QScriptValue Vector3df_prototype_getLength(QScriptContext *ctx, QScriptEngine *engine);
QScriptValue Vector3df_prototype_mul(QScriptContext *ctx, QScriptEngine *engine);
QScriptValue toScriptValueVector3(QScriptEngine *engine, const Vector3df &s)
{
    QScriptValue obj = engine->newObject();
    obj.setProperty("x", QScriptValue(engine, s.x));
    obj.setProperty("y", QScriptValue(engine, s.y));
    obj.setProperty("z", QScriptValue(engine, s.z));

    //this should suffice only once for the prototype somehow, but couldn't get that to work
    //ctorVector3df.property("prototype").setProperty("normalize", normalizeVector3df);
    obj.setProperty("normalize", engine->newFunction(Vector3df_prototype_normalize));
    obj.setProperty("length", engine->newFunction(Vector3df_prototype_getLength));
    obj.setProperty("mul", engine->newFunction(Vector3df_prototype_mul));

    return obj;
}

void fromScriptValueVector3(const QScriptValue &obj, Vector3df &s)
{
    s.x = (float)obj.property("x").toNumber();
    s.y = (float)obj.property("y").toNumber();
    s.z = (float)obj.property("z").toNumber();
}

QScriptValue Vector3df_prototype_normalize(QScriptContext *ctx, QScriptEngine *engine)
{
    Vector3df vec;
    fromScriptValueVector3(ctx->thisObject(), vec);
      
    return toScriptValueVector3(engine, vec.normalize());
}

QScriptValue Vector3df_prototype_getLength(QScriptContext *ctx, QScriptEngine *engine)
{
    Vector3df vec;
    fromScriptValueVector3(ctx->thisObject(), vec);
      
    return vec.getLength();
}

QScriptValue Vector3df_prototype_mul(QScriptContext *ctx, QScriptEngine *engine)
{
    if (ctx->argumentCount() != 1)
        return ctx->throwError("Vector3df mul() takes a single number argument.");
    if (!ctx->argument(0).isNumber())
        return ctx->throwError(QScriptContext::TypeError, "Vector3df mul(): argument is not a number");
    float scalar = ctx->argument(0).toNumber();
    //XXX add vec*vec
    
    Vector3df vec;
    fromScriptValueVector3(ctx->thisObject(), vec);

    return toScriptValueVector3(engine, vec * scalar);
}

QScriptValue Quaternion_prototype_ToEuler(QScriptContext *ctx, QScriptEngine *engine);
QScriptValue Quaternion_prototype_Normalize(QScriptContext *ctx, QScriptEngine *engine);
QScriptValue Quaternion_prototype_MakeIdentity(QScriptContext *ctx, QScriptEngine *engine);
QScriptValue toScriptValueQuaternion(QScriptEngine *engine, const Quaternion &s)
{
    QScriptValue obj = engine->newObject();
    obj.setProperty("x", QScriptValue(engine, s.x));
    obj.setProperty("y", QScriptValue(engine, s.y));
    obj.setProperty("z", QScriptValue(engine, s.z));
    obj.setProperty("w", QScriptValue(engine, s.w));
    return obj;
}

void fromScriptValueQuaternion(const QScriptValue &obj, Quaternion &s)
{
    s.x = (float)obj.property("x").toNumber();
    s.y = (float)obj.property("y").toNumber();
    s.z = (float)obj.property("z").toNumber();
    s.w = (float)obj.property("w").toNumber();
}

QScriptValue Quaternion_prototype_ToEuler(QScriptContext *ctx, QScriptEngine *engine)
{
    Quaternion quat;
    fromScriptValueQuaternion(ctx->thisObject(), quat);
    Vector3df eulerVal;
    quat.toEuler(eulerVal);
    // Convert to degree.
    eulerVal.x = RADTODEG * eulerVal.x;
    eulerVal.y = RADTODEG * eulerVal.y;
    eulerVal.z = RADTODEG * eulerVal.z;

    return toScriptValueVector3(engine, eulerVal);
}

QScriptValue Quaternion_prototype_Normalize(QScriptContext *ctx, QScriptEngine *engine)
{
    Quaternion quat;
    fromScriptValueQuaternion(ctx->thisObject(), quat);
    
    return toScriptValueQuaternion(engine, quat.normalize());
}

QScriptValue Quaternion_prototype_MakeIdentity(QScriptContext *ctx, QScriptEngine *engine)
{
    Quaternion quat;
    fromScriptValueQuaternion(ctx->thisObject(), quat);
    
    return toScriptValueQuaternion(engine, quat.makeIdentity());
}

QScriptValue toScriptValueTransform(QScriptEngine *engine, const Transform &s)
{
    QScriptValue obj = engine->newObject();
    obj.setProperty("pos", toScriptValueVector3(engine, s.position));
    obj.setProperty("rot", toScriptValueVector3(engine, s.rotation));
    obj.setProperty("scale", toScriptValueVector3(engine, s.scale));
    return obj;
}

void fromScriptValueTransform(const QScriptValue &obj, Transform &s)
{
    fromScriptValueVector3(obj.property("pos"), s.position);
    fromScriptValueVector3(obj.property("rot"), s.rotation);
    fromScriptValueVector3(obj.property("scale"), s.scale);
}

QScriptValue toScriptValueIAttribute(QScriptEngine *engine, IAttribute * const &s)
{
    QScriptValue obj = engine->newObject();
    if(s)
    {
        obj.setProperty("name", QScriptValue(engine, QString::fromStdString(s->GetNameString())));
        obj.setProperty("typename", QScriptValue(engine, QString::fromStdString(s->TypeName())));
        obj.setProperty("value", QScriptValue(engine, QString::fromStdString(s->ToString())));
    }
    else
    {
        LogError("Fail to get attribute values from IAttribute pointer, cause pointer was a null. returning empty object.");
    }
    return obj;
}

void fromScriptValueAssetReference(const QScriptValue &obj, AssetReference &s)
{
    s.ref = obj.property("ref").toString();
}

QScriptValue toScriptValueAssetReference(QScriptEngine *engine, const AssetReference &s)
{
    QScriptValue obj = engine->newObject();
    obj.setProperty("ref", QScriptValue(engine, s.ref));
    return obj;
}

void fromScriptValueAssetReferenceList(const QScriptValue &obj, AssetReferenceList &s)
{
    // Clear the old content as we are appending from the start!
    s.refs.clear();

    QScriptValueIterator it(obj);
    while(it.hasNext()) 
    {
        it.next();
        if (it.value().isString())
        {
            AssetReference reference(it.value().toString());
            s.Append(reference);
        }
    }
}

QScriptValue toScriptValueAssetReferenceList(QScriptEngine *engine, const AssetReferenceList &s)
{
    QScriptValue obj = engine->newObject();
    for(int i = 0; i < s.refs.size(); ++i)
        obj.setProperty(i, QScriptValue(engine, s[i].ref));
    return obj;
}

void fromScriptValueEntityList(const QScriptValue &obj, QList<Entity*> &ents)
{
    ents.clear();
    QScriptValueIterator it(obj);
    while(it.hasNext())
    {
        it.next();
        QObject *qent = it.value().toQObject();
        if (qent)
        {
            Entity *ent = qobject_cast<Entity*>(qent);
            if (ent)
                ents.append(ent);
        }
    }
}

QScriptValue toScriptValueEntityList(QScriptEngine *engine, const QList<Entity*> &ents)
{
    QScriptValue obj = engine->newArray(ents.size());
    for(int i=0; i<ents.size(); ++i)
    {
        Entity *ent = ents.at(i);
        if (ent)
            obj.setProperty(i, engine->newQObject(ent));
    }
    return obj;
}

void fromScriptValueEntitStdyList(const QScriptValue &obj, EntityList &ents)
{
    ents.clear();
    QScriptValueIterator it(obj);
    while(it.hasNext())
    {
        it.next();
        QObject *qent = it.value().toQObject();
        if (qent)
        {
            Entity *ent = qobject_cast<Entity*>(qent);
            if (ent)
                ents.push_back(ent->shared_from_this());
        }
    }
}

QScriptValue toScriptValueEntityStdList(QScriptEngine *engine, const EntityList &ents)
{
    QScriptValue obj = engine->newArray();
    std::list<EntityPtr>::const_iterator iter = ents.begin();
    int i = 0;
    while(iter != ents.end())
    {
        EntityPtr entPtr = (*iter);
        if (entPtr.get())
        {
            obj.setProperty(i, engine->newQObject(entPtr.get()));
            i++;
        }
        ++iter;
    }
    return obj;
}

void fromScriptValueStdString(const QScriptValue &obj, std::string &s)
{
    s = obj.toString().toStdString();
}

QScriptValue toScriptValueStdString(QScriptEngine *engine, const std::string &s)
{
    return engine->newVariant(QString::fromStdString(s));
}

void fromScriptValueIAttribute(const QScriptValue &obj, IAttribute *&s)
{
}

QScriptValue createColor(QScriptContext *ctx, QScriptEngine *engine)
{
    Color newColor;
    if (ctx->argumentCount() == 3) //RGB
    {
        if (ctx->argument(0).isNumber() &&
            ctx->argument(1).isNumber() &&
            ctx->argument(2).isNumber())
        {
            newColor.r = (float)ctx->argument(0).toNumber();
            newColor.g = (float)ctx->argument(1).toNumber();
            newColor.b = (float)ctx->argument(2).toNumber();
        }
        else
            return ctx->throwError(QScriptContext::TypeError, "Color(): arguments aren't numbers.");
    }
    else if(ctx->argumentCount() == 4) //RGBA
    {
        if (ctx->argument(0).isNumber() &&
            ctx->argument(1).isNumber() &&
            ctx->argument(2).isNumber() &&
            ctx->argument(3).isNumber())
        {
            newColor.r = (float)ctx->argument(0).toNumber();
            newColor.g = (float)ctx->argument(1).toNumber();
            newColor.b = (float)ctx->argument(2).toNumber();
            newColor.a = (float)ctx->argument(3).toNumber();
        }
        else
            return ctx->throwError(QScriptContext::TypeError, "Color(): arguments aren't numbers.");
    }
    return engine->toScriptValue(newColor);
}

QScriptValue createVector3df(QScriptContext *ctx, QScriptEngine *engine)
{
    Vector3df newVec;
    if (ctx->argumentCount() == 3)
    {
        if (ctx->argument(0).isNumber() &&
            ctx->argument(1).isNumber() &&
            ctx->argument(2).isNumber())
        {
            newVec.x = (f32)ctx->argument(0).toNumber();
            newVec.y = (f32)ctx->argument(1).toNumber();
            newVec.z = (f32)ctx->argument(2).toNumber();
        }
        else
            return ctx->throwError(QScriptContext::TypeError, "Vector3df(): arguments aren't numbers.");
    }
    QScriptValue returnValue = engine->toScriptValue(newVec);
    // Expose native functions to javascript.
    returnValue.setProperty("Normalize", engine->newFunction(Vector3df_prototype_normalize));
    returnValue.setProperty("Length", engine->newFunction(Vector3df_prototype_getLength));
    returnValue.setProperty("Mul", engine->newFunction(Vector3df_prototype_mul));
    return returnValue;
}

QScriptValue createQuaternion(QScriptContext *ctx, QScriptEngine *engine)
{
    Quaternion newQuat;
    if (ctx->argumentCount() == 3)
    {
        if (ctx->argument(0).isNumber() &&
            ctx->argument(1).isNumber() &&
            ctx->argument(2).isNumber())
        {
            newQuat.set(DEGTORAD * (f32)ctx->argument(0).toNumber(),
                        DEGTORAD * (f32)ctx->argument(1).toNumber(),
                        DEGTORAD * (f32)ctx->argument(2).toNumber());
        }
        else
            return ctx->throwError(QScriptContext::TypeError, "Quaternion(): arguments aren't numbers.");
    }
    QScriptValue retVal = engine->toScriptValue(newQuat);
    // Expose native functions to javascript.
    retVal.setProperty("ToEuler", engine->newFunction(Quaternion_prototype_ToEuler));
    retVal.setProperty("Normalize", engine->newFunction(Quaternion_prototype_Normalize));
    retVal.setProperty("MakeIdentity", engine->newFunction(Quaternion_prototype_MakeIdentity));

    return retVal;
}

QScriptValue createTransform(QScriptContext *ctx, QScriptEngine *engine)
{
    Transform newTransform;
    if (ctx->argumentCount() == 3) // Support three Vector3df as arguments.
    {
        //! todo! Figure out how this could be more safe.
        if (ctx->argument(0).isObject() &&
            ctx->argument(1).isObject() &&
            ctx->argument(2).isObject())
        {
            Vector3df pos = engine->fromScriptValue<Vector3df>(ctx->argument(0));
            Vector3df rot = engine->fromScriptValue<Vector3df>(ctx->argument(1));
            Vector3df scale = engine->fromScriptValue<Vector3df>(ctx->argument(2));
            newTransform.position = pos;
            newTransform.rotation = rot;
            newTransform.scale = scale;
        }
    }
    return engine->toScriptValue(newTransform);
}

QScriptValue createAssetReference(QScriptContext *ctx, QScriptEngine *engine)
{
    AssetReference newAssetRef;
    if (ctx->argumentCount() == 2) // Both ref and it's type are given as arguments.
        if (ctx->argument(0).isString() &&
            ctx->argument(1).isString())
        {
            newAssetRef.ref = ctx->argument(0).toString();
            newAssetRef.type = ctx->argument(0).toString();
        }
    else if(ctx->argumentCount() == 1) // Only ref.
        if (ctx->argument(0).isString())
            newAssetRef.ref = ctx->argument(0).toString();
    return engine->toScriptValue(newAssetRef);
}

QScriptValue createAssetReferenceList(QScriptContext *ctx, QScriptEngine *engine)
{
    AssetReferenceList newAssetRefList;
    if (ctx->argumentCount() >= 1)
    {
        if(ctx->argument(0).isArray())
            fromScriptValueAssetReferenceList(ctx->argument(0), newAssetRefList);
        else
            return ctx->throwError(QScriptContext::TypeError, "AssetReferenceList(): argument 0 type isn't array.");
        if (ctx->argumentCount() == 2)
        {
            if(ctx->argument(1).isString())
                newAssetRefList.type = ctx->toString();
            else
                return ctx->throwError(QScriptContext::TypeError, "AssetReferenceList(): argument 1 type isn't string.");
        }
    }
    return engine->toScriptValue(newAssetRefList);
}

void RegisterCoreMetaTypes()
{
    qRegisterMetaType<ScenePtr>("ScenePtr");
    qRegisterMetaType<ComponentPtr>("ComponentPtr");
    qRegisterMetaType<Color>("Color");
    qRegisterMetaType<Vector3df>("Vector3df");
    qRegisterMetaType<Quaternion>("Quaternion");
    qRegisterMetaType<Transform>("Transform");
    qRegisterMetaType<AssetReference>("AssetReference");
    qRegisterMetaType<AssetReferenceList>("AssetReferenceList");
    qRegisterMetaType<IAttribute*>("IAttribute*");
    qRegisterMetaType<QList<Entity*> >("QList<Entity*>");
    qRegisterMetaType<EntityList>("EntityList");
    qRegisterMetaType<std::string>("std::string");
}

void ExposeCoreTypes(QScriptEngine *engine)
{
    qScriptRegisterMetaType(engine, toScriptValueColor, fromScriptValueColor);
    qScriptRegisterMetaType(engine, toScriptValueVector3, fromScriptValueVector3);
    qScriptRegisterMetaType(engine, toScriptValueQuaternion, fromScriptValueQuaternion);
    qScriptRegisterMetaType(engine, toScriptValueTransform, fromScriptValueTransform);
    qScriptRegisterMetaType(engine, toScriptValueAssetReference, fromScriptValueAssetReference);
    qScriptRegisterMetaType(engine, toScriptValueAssetReferenceList, fromScriptValueAssetReferenceList);

    int id = qRegisterMetaType<ScenePtr>("ScenePtr");
    qScriptRegisterMetaType_helper(
        engine, id, reinterpret_cast<QScriptEngine::MarshalFunction>(qScriptValueFromBoostSharedPtr<SceneManager>),
        reinterpret_cast<QScriptEngine::DemarshalFunction>(qScriptValueToBoostSharedPtr<SceneManager>),
        QScriptValue());

    qScriptRegisterMetaType<ComponentPtr>(engine, qScriptValueFromBoostSharedPtr, qScriptValueToBoostSharedPtr);

    qScriptRegisterMetaType<IAttribute*>(engine, toScriptValueIAttribute, fromScriptValueIAttribute);
    qScriptRegisterMetaType<ScenePtr>(engine, qScriptValueFromBoostSharedPtr, qScriptValueToBoostSharedPtr);
    qScriptRegisterMetaType<EntityPtr>(engine, qScriptValueFromBoostSharedPtr, qScriptValueToBoostSharedPtr);
    qScriptRegisterMetaType<ComponentPtr>(engine, qScriptValueFromBoostSharedPtr, qScriptValueToBoostSharedPtr);
    qScriptRegisterMetaType<QList<Entity*> >(engine, toScriptValueEntityList, fromScriptValueEntityList);
    qScriptRegisterMetaType<EntityList>(engine, toScriptValueEntityStdList, fromScriptValueEntitStdyList);
    qScriptRegisterMetaType<std::string>(engine, toScriptValueStdString, fromScriptValueStdString);
    
    // Register constructors
    QScriptValue ctorVector3 = engine->newFunction(createVector3df);
    engine->globalObject().setProperty("Vector3df", ctorVector3);
    QScriptValue ctorColor = engine->newFunction(createColor);
    engine->globalObject().setProperty("Color", ctorColor);
    QScriptValue ctorTransform = engine->newFunction(createTransform);
    engine->globalObject().setProperty("Transform", ctorTransform);
    QScriptValue ctorAssetReference = engine->newFunction(createAssetReference);
    engine->globalObject().setProperty("AssetReference", ctorAssetReference);
    QScriptValue ctorAssetReferenceList = engine->newFunction(createAssetReferenceList);
    engine->globalObject().setProperty("AssetReferenceList", ctorAssetReferenceList);

    // Register both constructors and methods (with js prototype style)
    // http://doc.qt.nokia.com/latest/scripting.html#prototype-based-programming-with-the-qtscript-c-api
    /* doesn't work for some reason, is now hacked in toScriptValue to every instance (bad!) */
    /*QScriptValue protoVector3df = engine->newObject();
    protoVector3df.setProperty("normalize2", engine->newFunction(Vector3df_prototype_normalize)); //leaving in for debug/test purposes
    QScriptValue ctorVector3df = engine->newFunction(createVector3df, protoVector3df); //this is supposed to work according to docs, doesnt.
    engine->globalObject().setProperty("Vector3df", ctorVector3df);*/
    
    QScriptValue ctorQuaternion = engine->newFunction(createQuaternion);
    engine->globalObject().setProperty("Quaternion", ctorQuaternion);
}

/*
    Copyright (c) 2013 Timothy Reaves treaves@silverfieldstech.com

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any
    later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this library.  If not, see <http://www.gnu.org/licenses/>.
  */
#include "classhandlermanager.h"

#include "classhandler.h"

#include <QtCore/QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QMetaObject>
#include <QMetaMethod>
#include <QMetaType>
#include <QtPlugin>
#include <QPluginLoader>
#include <QSet>
#include <QString>
#include <QUrl>
#include <QVariant>

#include "httpserverrequest.h"
#include "headers.h"

namespace Tufao {

// Initialize static members.
QStringList ClassHandlerManager::pluginLocations;

/* ****************************************************************************************************************** */
#pragma mark -
#pragma mark Object lifecycle
/* ****************************************************************************************************************** */
ClassHandlerManager::ClassHandlerManager(QString pluginID, QString context, QObject * parent) :
    QObject(parent),
    pluginID(pluginID),
    m_context(context)
{
    // Set up the search locations
    if(pluginLocations.isEmpty()) {
        // Add standard locations to pluginLocations
        // First, the typical app config dir's
#ifdef Q_OS_WIN
        // Code can be added here, but, for the time being, I'm leaving it empty.
#elif defined(Q_OS_MAC)
        pluginLocations.append(QDir::homePath() + "/Library/Application Support/Tufao");
        pluginLocations.append("/Library/Application Support/Tufao");
#else
        pluginLocations.append(QDir::homePath() + "/.tufao");
#endif

        // The standard library locations
        foreach (QString libraryPath, QCoreApplication::libraryPaths()) {
            QDir testDir(libraryPath + QDir::separator() + "Tufao");
            if(testDir.exists()){
                pluginLocations.append(testDir.absolutePath());
            }
        }

        // Finally, the install location
        QFileInfo installDir(QCoreApplication::applicationDirPath());
        if(installDir.isDir()) {
            pluginLocations.append(installDir.absolutePath());
        }

        qDebug() << "pluginLocations:" << pluginLocations;
    }

    // Now load the plugin of interest
    // First list all static plugins.
    foreach (QObject * pluginInterface, QPluginLoader::staticInstances()){
        ClassHandler * plugin = qobject_cast<ClassHandler *>(pluginInterface);
        if (plugin){
            registerHandler(plugin);
        }
    }
///
    // Then list dynamic libraries from the plugins/ directory
    QStringList contents;
#ifdef Q_OS_WIN
    QString pluginExtension = ".dll";
#else
#ifdef Q_OS_MAC
    QString pluginExtension = ".dylib";
#else
    QString pluginExtension = ".so";
#endif
#endif
    // retrieve a list of all dynamic libraries from the search paths
    foreach (QString path, pluginLocations) {
        QFileInfo thisPath(QDir(path).filePath("plugins"));
        if(thisPath.isDir()){
            QDir thisDir(thisPath.absoluteFilePath());
            qDebug() << "Search " << thisPath.absolutePath() << " for plugins.";
            foreach (const QString entry, thisDir.entryList()) {
                if (!(entry == "..") && !(entry == ".") && entry.endsWith(pluginExtension)){
                    contents.append(thisDir.filePath(entry));
                }
            }
        }
    }

    // Check each dynamic library to see if it is a plugin
    foreach (QString pluginPath, contents) {
        QPluginLoader loader(pluginPath);
        // If we were constructed with a pluginID, we need to chech each plugin.
        if(pluginID.isEmpty() || pluginID == loader.metaData().value("IID").toString()) {
            if (!loader.load()) {
                qWarning() << "Couldn't load the dynamic library: "
                           << QDir::toNativeSeparators(pluginPath)
                           << ": "
                           << loader.errorString();
                continue;
            }

            QObject* obj = loader.instance();
            if (!obj) {
                qWarning() << "Couldn't open the dynamic library: "
                           << QDir::toNativeSeparators(pluginPath)
                           << ": "
                           << loader.errorString();
                continue;
            }

            ClassHandler * plugin = qobject_cast<ClassHandler *>(obj);
            if (plugin) {
                if (plugin){
                    registerHandler(plugin);
                }
            }
        }

    }
}

ClassHandlerManager::~ClassHandlerManager()
{
    foreach (ClassHandlerManager::PluginDescriptor * descriptor, handlers) {
        delete descriptor;
    }
    handlers.clear();
}

/* ****************************************************************************************************************** */
#pragma mark -
#pragma mark Accessors & mutators
/* ****************************************************************************************************************** */
QString ClassHandlerManager::context(void) const
{
    return m_context;
}

/* ****************************************************************************************************************** */
#pragma mark -
#pragma mark Static Methods
/* ****************************************************************************************************************** */
void ClassHandlerManager::addPluginLocation(const QString location)
{
    if(!pluginLocations.contains(location)){
        pluginLocations.append(location);
    }
}

/* ****************************************************************************************************************** */
#pragma mark -
#pragma mark Private Methods
/* ****************************************************************************************************************** */
void ClassHandlerManager::dispatchVoidMethod(QMetaMethod method,
                                             ClassHandler * handler,
                                             const QGenericArgument * args) const
{
    method.invoke(handler,
                  Qt::DirectConnection,
                  args[0],
                  args[1],
                  args[2],
                  args[3],
                  args[4],
                  args[5],
                  args[6],
                  args[7],
                  args[8],
                  args[9]
                  );
}

void ClassHandlerManager::dispatchJSONMethod(HttpServerResponse & response,
                                             QMetaMethod method,
                                             ClassHandler *handler,
                                             const QGenericArgument *args) const
{
    QJsonObject result;
    bool wasInvoked = method.invoke(handler,
                                    Qt::DirectConnection,
                                    Q_RETURN_ARG(QJsonObject, result),
                                    args[0],
                                    args[1],
                                    args[2],
                                    args[3],
                                    args[4],
                                    args[5],
                                    args[6],
                                    args[7],
                                    args[8],
                                    args[9]
                                    );
    if(wasInvoked) {
        HttpResponseStatus status = HttpResponseStatus::OK;
        QJsonDocument jsonDocument;
        if(result.contains(HttpResponseStatusKey)) {
            status = HttpResponseStatus(result[HttpResponseStatusKey].toInt());
            //The response will either be an JsonObject, or a JsonArray
            if(result[JsonResponseKey].isArray()) {
                jsonDocument.setArray(result[JsonResponseKey].toArray());
            } else {
                jsonDocument = QJsonDocument(result[JsonResponseKey].toObject());
            }
        }
        response.writeHead(status);
        response.headers().replace("Content-Type", "application/json");
        response.end(jsonDocument.toJson());
    }
}

bool ClassHandlerManager::processRequest(HttpServerRequest & request,
                                         HttpServerResponse & response,
                                         const QString className,
                                         const QString methodName,
                                         const QHash<QString, QString> arguments)
{
    bool handled = false;
    bool canHandle = true;
    int methodIndex = selectMethod(className, methodName, arguments);
    if(methodIndex > -1) {
        ClassHandlerManager::PluginDescriptor * handler = handlers[className];
        QMetaMethod method = handler->handler->metaObject()->method(methodIndex);

        // Create the arguments
        QGenericArgument argumentTable[10];
        argumentTable[0] = Q_ARG(Tufao::HttpServerRequest, request);
        argumentTable[1] = Q_ARG(Tufao::HttpServerResponse, response);

        // We need this to keep objects in scope until the actual invoke() call.
        QVariant variants[10];

        int argumentIndex = 2;
        while(argumentIndex < method.parameterCount()){
            QString parameterName = method.parameterNames()[argumentIndex];
            // qDebug() << "Processing " << parameterName;
            variants[argumentIndex] = QVariant::fromValue(arguments.value(parameterName));
            int methodType = method.parameterType(argumentIndex);
            if(variants[argumentIndex].canConvert(methodType)) {
                variants[argumentIndex].convert(methodType);
                argumentTable[argumentIndex] = QGenericArgument(variants[argumentIndex].typeName(),
                                                                variants[argumentIndex].data());
                // qDebug() << "Converted "
                //          << arguments.value(parameterName)
                //          << " to type "
                //          << QVariant::typeToName(methodType)
                //          << " index "
                //          << argumentIndex;
            } else {
                qWarning() << "Can not convert "
                           << arguments.value(parameterName)
                           << " to type " << QVariant::typeToName(methodType);
            }
            argumentIndex+=1;
        }
        if(canHandle) {
            if(method.returnType() == QMetaType::QJsonObject) {
                this->dispatchJSONMethod(response, method, handler->handler, argumentTable);
            } else {
                this->dispatchVoidMethod(method, handler->handler, argumentTable);
            }
            handled = true;
        }
    } else {
        qWarning() << "Cound not find a method named with a matching signature.";
    }

    return handled;
}

void ClassHandlerManager::registerHandler(ClassHandler * handler)
{
    // Only process plugins that have not already been registered.
    if (!handlers.contains(handler->objectName())){
        qDebug() << "Registering " << handler->objectName() << " as a handler.";
        bool canDispathTo = false;
        const QMetaObject* metaObject = handler->metaObject();
        for(int methodIndex = metaObject->methodOffset(); methodIndex < metaObject->methodCount(); ++methodIndex) {
            QMetaMethod method = metaObject->method(methodIndex);
            // We only want public slots whos first two arguements are request & response
            if(method.methodType() == QMetaMethod::Slot && method.access() == QMetaMethod::Public) {
                QList<QByteArray> parameterNames = method.parameterNames();
                if(parameterNames[0] == QByteArray("request") && parameterNames[1] == QByteArray("response")) {
                    canDispathTo = true;
                    ClassHandlerManager::PluginDescriptor * pluginDescriptor = handlers[handler->objectName()];
                    if(pluginDescriptor == NULL) {
                        pluginDescriptor = new ClassHandlerManager::PluginDescriptor();
                        handlers[handler->objectName()] = pluginDescriptor;
                    }
                    pluginDescriptor->className = handler->objectName();
                    pluginDescriptor->handler = handler;

                    uint parameterHash = qHash(QString::fromLatin1(method.name()));
                    foreach (QByteArray nameBytes, parameterNames) {
                        parameterHash += qHash(QString::fromLatin1(nameBytes));
                    }
                    pluginDescriptor->methods.insert(parameterHash, methodIndex);
                    pluginDescriptor->methodNames.append(QString::fromLatin1(method.name()));

                    QString signature = QString::fromLatin1(method.methodSignature());
                    qDebug() << signature << " is a dispatchable endpoint.";
                }
            }
        }
        if(canDispathTo) {
            handler->init();
        }
    }

}

int ClassHandlerManager::selectMethod(const QString className,
                                      const QString methodName,
                                      const QHash<QString, QString> arguments) const
{
    int methodIndex = -1;
    uint parameterHash = qHash(methodName);
    parameterHash += qHash(QString("request"));
    parameterHash += qHash(QString("response"));
    foreach (QString key, arguments.keys()) {
        parameterHash += qHash(key);
    }
    PluginDescriptor * pluginDescriptor = handlers[className];
    if (pluginDescriptor->methods.contains(parameterHash)) {
        methodIndex = pluginDescriptor->methods.value(parameterHash);
    }
    return methodIndex;
}

/* ****************************************************************************************************************** */
#pragma mark -
#pragma mark Override Tufao::AbstractHttpServerRequestHandler Methods
/* ****************************************************************************************************************** */
bool ClassHandlerManager::handleRequest(Tufao::HttpServerRequest & request, Tufao::HttpServerResponse & response)
{
    bool wasHandled = false;
    QStringList pathComponents = request.url().toString().split("/", QString::SkipEmptyParts);


    //Is the request for our context?
    bool useContext = !m_context.isEmpty();
    // There must be at least two path components (class & method), and 3 if a context is specified.
    int minimumPathComponents = useContext ? 3 : 2;
    if (pathComponents.length() < minimumPathComponents) {
        qWarning() << "Request was dispatched to handler, but too few path components found.  The path components are"
                   << pathComponents;
    } else if(pathComponents.length() > minimumPathComponents  + 16) {
        // We also can not have too many arguments; 16 is max, as that is 8 argumetns plus request & response
        qWarning() << "Request was dispatched to handler, but too many path components found.  The path components are"
                   << pathComponents;
    } else {
        if(!useContext || m_context == pathComponents[0]) {
            // Add the context to the request
            request.setContext(m_context);

            int pathIndex = useContext ? 1 : 0;
            QString className = pathComponents[pathIndex++];
            QString methodName = pathComponents[pathIndex++];
            // We need to have an even number of path components left
            if((pathComponents.length() - pathIndex) % 2 == 0) {
                // See if we have a class handler with a matching method
                if(handlers.contains(className) && handlers[className]->methodNames.contains(methodName)) {
                    // Convert the remaining path components into an argument hash
                    QHash<QString, QString> arguments;
                    while(pathIndex < pathComponents.length()){
                        arguments[pathComponents[pathIndex]] = pathComponents[pathIndex + 1];
                        pathIndex += 2;
                    }
                    wasHandled = processRequest(request, response, className, methodName, arguments);
                } else {
                    if(handlers.contains(className)) {
                        qWarning() << "The class" << className << "has no method named" << methodName;
                    }
                }
            } else {
                qWarning() << "Can not dispath as an odd number of parameter components were supplied.";
            }

        }
    }

    return wasHandled;
}

} // namespace Tufao

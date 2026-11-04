  #pragma once
#include <QtPlugin>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>

// Native C++ Plugin Interface for Keter AI Studio (.dll / .dylib / .so)
class IKeterPlugin {
public:
    virtual ~IKeterPlugin() = default;

    // Metadata
    virtual QString pluginName() const = 0;
    virtual QString pluginVersion() const = 0;
    virtual QString pluginAuthor() const = 0;
    virtual QString targetRuntime() const = 0; // e.g., "Native C++", "Godot 4", "Unreal Engine 5"

    // Returns MCP-compatible JSON Schema tool definitions provided by this C++ plugin
    virtual QJsonArray registeredMcpTools() const = 0;

    // Executes a tool call invoked by the AI model or external Game Engine via MCP
    virtual QJsonObject executeTool(const QString& toolName, const QJsonObject& arguments) = 0;
};

#define IKeterPlugin_iid "org.addevelopers.keter.IKeterPlugin/1.0"
Q_DECLARE_INTERFACE(IKeterPlugin, IKeterPlugin_iid)

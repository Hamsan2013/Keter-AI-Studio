  #pragma once
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <functional>
#include "IKeterPlugin.h"

class McpServer : public QTcpServer {
    Q_OBJECT
public:
    using AiInferenceCallback = std::function<QString(const QString& prompt)>;

    explicit McpServer(QObject* parent = nullptr) : QTcpServer(parent) {}

    void setInferenceCallback(AiInferenceCallback cb) {
        m_inferenceCallback = std::move(cb);
    }

    void registerCppPlugin(IKeterPlugin* plugin) {
        if (plugin) m_plugins.append(plugin);
    }

    bool startMcp(quint16 port = 8765) {
        if (isListening()) close();
        bool ok = listen(QHostAddress::LocalHost, port);
        if (ok) {
            emit serverLog(QString("[%1] MCP Server listening on http://127.0.0.1:%2/mcp (JSON-RPC 2.0)")
                               .arg(QDateTime::currentDateTime().toString("HH:mm:ss"))
                               .arg(port));
        }
        return ok;
    }

    void stopMcp() {
        if (isListening()) {
            close();
            emit serverLog("[MCP] Server stopped.");
        }
    }

signals:
    void serverLog(const QString& logLine);
    void engineConnected(const QString& engineName);

protected:
    void incomingConnection(qintptr socketDescriptor) override {
        QTcpSocket* socket = new QTcpSocket(this);
        socket->setSocketDescriptor(socketDescriptor);

        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
            QByteArray rawReq = socket->readAll();
            int bodyIdx = rawReq.indexOf("\r\n\r\n");
            QByteArray body = (bodyIdx != -1) ? rawReq.mid(bodyIdx + 4) : rawReq;

            QJsonParseError err;
            QJsonDocument doc = QJsonDocument::fromJson(body, &err);
            QJsonObject responseObj;

            if (err.error == QJsonParseError::NoError && doc.isObject()) {
                responseObj = handleJsonRpc(doc.object());
            } else {
                responseObj["jsonrpc"] = "2.0";
                responseObj["error"] = QJsonObject{{"code", -32700}, {"message", "Parse error"}};
            }

            QByteArray payload = QJsonDocument(responseObj).toJson(QJsonDocument::Compact);
            QByteArray httpResp = "HTTP/1.1 200 OK\r\n"
                                  "Content-Type: application/json\r\n"
                                  "Access-Control-Allow-Origin: *\r\n"
                                  "Content-Length: " + QByteArray::number(payload.size()) + "\r\n"
                                  "Connection: close\r\n\r\n" + payload;
            socket->write(httpResp);
            socket->flush();
            socket->disconnectFromHost();
        });

        connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
    }

private:
    QList<IKeterPlugin*> m_plugins;
    AiInferenceCallback m_inferenceCallback;

    QJsonObject handleJsonRpc(const QJsonObject& req) {
        QString method = req["method"].toString();
        QJsonValue id = req["id"];
        QJsonObject params = req["params"].toObject();

        emit serverLog(QString("[MCP Request] method: %1 | id: %2").arg(method, id.toVariant().toString()));

        QJsonObject res;
        res["jsonrpc"] = "2.0";
        res["id"] = id;

        if (method == "initialize") {
            QString clientName = params["clientInfo"].toObject()["name"].toString("External Engine");
            emit engineConnected(clientName);
            res["result"] = QJsonObject{
                {"protocolVersion", "2025-06-18"},
                {"serverInfo", QJsonObject{{"name", "Keter-AI-Studio-MCP"}, {"version", "1.0.0"}}},
                {"capabilities", QJsonObject{{"tools", QJsonObject{{"listChanged", true}}}}}
            };
        } else if (method == "tools/list") {
            QJsonArray tools = builtInGameEngineTools();
            for (auto* plugin : m_plugins) {
                for (const auto& t : plugin->registeredMcpTools()) tools.append(t);
            }
            res["result"] = QJsonObject{{"tools", tools}};
        } else if (method == "tools/call") {
            QString toolName = params["name"].toString();
            QJsonObject args = params["arguments"].toObject();
            res["result"] = executeMcpTool(toolName, args);
        } else {
            res["error"] = QJsonObject{{"code", -32601}, {"message", "Method not found: " + method}};
        }
        return res;
    }

    QJsonArray builtInGameEngineTools() const {
        return QJsonArray{
            QJsonObject{
                {"name", "godot_generate_script"},
                {"description", "Generates GDScript or C++ GDExtension code for Godot 4 nodes and scenes."},
                {"inputSchema", QJsonObject{
                    {"type", "object"},
                    {"properties", QJsonObject{
                        {"node_type", QJsonObject{{"type", "string"}}},
                        {"behavior", QJsonObject{{"type", "string"}}}
                    }}
                }}
            },
            QJsonObject{
                {"name", "unreal_actor_assist"},
                {"description", "Generates Unreal Engine 5 C++ Actor/Component logic or NPC dialogue trees."},
                {"inputSchema", QJsonObject{
                    {"type", "object"},
                    {"properties", QJsonObject{
                        {"class_name", QJsonObject{{"type", "string"}}},
                        {"prompt", QJsonObject{{"type", "string"}}}
                    }}
                }}
            },
            QJsonObject{
                {"name", "keter_local_chat"},
                {"description", "Sends a prompt directly to the active local GGUF model in Keter AI Studio."},
                {"inputSchema", QJsonObject{
                    {"type", "object"},
                    {"properties", QJsonObject{{"prompt", QJsonObject{{"type", "string"}}}}}
                }}
            }
        };
    }

    QJsonObject executeMcpTool(const QString& name, const QJsonObject& args) {
        // Check loaded C++ plugins first
        for (auto* plugin : m_plugins) {
            QJsonObject out = plugin->executeTool(name, args);
            if (!out.isEmpty()) return out;
        }

        QString prompt;
        if (name == "godot_generate_script") {
            prompt = QString("Write a complete Godot 4 GDScript extending %1 for: %2")
                         .arg(args["node_type"].toString("CharacterBody2D"), args["behavior"].toString());
        } else if (name == "unreal_actor_assist") {
            prompt = QString("Write Unreal Engine 5 C++ code for class %1: %2")
                         .arg(args["class_name"].toString("AKeterActor"), args["prompt"].toString());
        } else {
            prompt = args["prompt"].toString();
        }

        QString aiReply = m_inferenceCallback ? m_inferenceCallback(prompt)
                                              : "Local GGUF inference engine ready.";
        return QJsonObject{
            {"content", QJsonArray{QJsonObject{{"type", "text"}, {"text", aiReply}}}}
        };
    }
};

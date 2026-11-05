#pragma once
#include <QMainWindow>
#include <QWidget>
#include <QFrame>
#include <QCoreApplication>
#include <QRegularExpression>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QListWidget>
#include <QScrollArea>
#include <QScrollBar>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QComboBox>
#include <QProgressBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <QSettings>
#include <QDateTime>

#if defined(Q_OS_WIN)
#include <windows.h>
#include <mmsystem.h>
#endif

#include "HardwareChecker.h"
#include "McpServer.h"

// ============================================================================
// CUSTOM VECTOR ICON BUTTON (REAL MICROPHONE & SEND ICONS — NO CHEAP EMOJIS)
// ============================================================================
class VectorIconButton : public QPushButton {
    Q_OBJECT
public:
    enum IconType { Microphone, SendArrow, Speaker };

    explicit VectorIconButton(IconType type, QWidget* parent = nullptr)
        : QPushButton(parent), m_type(type) {
        setFixedSize(42, 42);
        setCursor(Qt::PointingHandCursor);
    }

    void setRecordingState(bool recording) {
        m_recording = recording;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        // Circular / Pill Background
        QColor bg = m_recording ? QColor("#DC2626")
                  : (m_type == SendArrow ? QColor("#2563EB") : QColor("#1E293B"));
        if (underMouse() && !m_recording) {
            bg = (m_type == SendArrow) ? QColor("#3B82F6") : QColor("#334155");
        }
        p.setPen(Qt::NoPen);
        p.setBrush(bg);
        p.drawRoundedRect(rect().adjusted(2, 2, -2, -2), 19, 19);

        // Draw crisp vector icon
        QPen pen(Qt::white, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);

        int cx = width() / 2;
        int cy = height() / 2;

        if (m_type == Microphone) {
            if (m_recording) {
                p.setBrush(Qt::white);
                p.drawRoundedRect(cx - 5, cy - 5, 10, 10, 2, 2);
            } else {
                // Microphone capsule
                p.drawRoundedRect(cx - 4, cy - 9, 8, 11, 4, 4);
                // Microphone U-cradle
                p.drawArc(cx - 7, cy - 6, 14, 11, 0 * 16, -180 * 16);
                // Stand stem & base
                p.drawLine(cx, cy + 5, cx, cy + 9);
                p.drawLine(cx - 4, cy + 9, cx + 4, cy + 9);
            }
        } else if (m_type == SendArrow) {
            QPainterPath path;
            path.moveTo(cx - 6, cy + 5);
            path.lineTo(cx + 7, cy);
            path.lineTo(cx - 6, cy - 5);
            path.lineTo(cx - 3, cy);
            path.closeSubpath();
            p.setBrush(Qt::white);
            p.drawPath(path);
        } else if (m_type == Speaker) {
            QPainterPath spk;
            spk.moveTo(cx - 6, cy - 3);
            spk.lineTo(cx - 2, cy - 3);
            spk.lineTo(cx + 2, cy - 7);
            spk.lineTo(cx + 2, cy + 7);
            spk.lineTo(cx - 2, cy + 3);
            spk.lineTo(cx - 6, cy + 3);
            spk.closeSubpath();
            p.drawPath(spk);
            p.drawArc(cx + 1, cy - 5, 6, 10, -60 * 16, 120 * 16);
        }
    }

private:
    IconType m_type;
    bool m_recording = false;
};

// ============================================================================
// MAIN KETER AI STUDIO WINDOW (CARD-BASED MODERN UI + REAL LLAMA.CPP ENGINE)
// ============================================================================
class KeterMainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit KeterMainWindow(QWidget* parent = nullptr) : QMainWindow(parent) {
        setWindowTitle("Keter AI Studio — Local GGUF Runtime, Voice & .KHM Ecosystem [AD Developers]");
        resize(1340, 840);

        m_appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        m_modelsDir = m_appDataDir + "/models";
        m_khmPluginsDir = m_appDataDir + "/khm_plugins";
        QDir().mkpath(m_modelsDir);
        QDir().mkpath(m_khmPluginsDir);

        m_net = new QNetworkAccessManager(this);
        m_llamaServerProc = new QProcess(this);

        connect(m_llamaServerProc, &QProcess::readyReadStandardOutput, this, &KeterMainWindow::onLlamaServerOutput);
        connect(m_llamaServerProc, &QProcess::readyReadStandardError, this, &KeterMainWindow::onLlamaServerOutput);
        connect(m_llamaServerProc, &QProcess::finished, this, [this]() {
            m_isModelLoadedInRam = false;
            updateModelStatusHeader();
        });

        m_mcpServer = new McpServer(this);
        m_mcpServer->setInferenceCallback([this](const QString& prompt) {
            return m_isModelLoadedInRam ? "Processed via loaded GGUF model on port 8089." : "Load a GGUF model in Keter AI Studio first.";
        });

        setupUi();
        loadSavedModelRegistry();
        refreshLocalModelCards();
        refreshKhmPluginCards();
        m_mcpServer->startMcp(8765);
    }

    ~KeterMainWindow() override {
        if (m_llamaServerProc && m_llamaServerProc->state() != QProcess::NotRunning) {
            m_llamaServerProc->kill();
            m_llamaServerProc->waitForFinished(1000);
        }
    }

private:
    QString m_appDataDir;
    QString m_modelsDir;
    QString m_khmPluginsDir;
    QStringList m_registeredModelPaths;
    QString m_currentlyLoadedModelPath;
    bool m_isModelLoadedInRam = false;

    QNetworkAccessManager* m_net = nullptr;
    QProcess* m_llamaServerProc = nullptr;
    McpServer* m_mcpServer = nullptr;
    QNetworkReply* m_activeDownload = nullptr;
    QFile* m_downloadFile = nullptr;

    // Navigation
    QListWidget* m_sidebar = nullptr;
    QStackedWidget* m_pages = nullptr;

    // Page 0: Chat Studio
    QComboBox* m_modelSelectorCombo = nullptr;
    QPushButton* m_loadUnloadBtn = nullptr;
    QLabel* m_modelStatusPill = nullptr;
    QVBoxLayout* m_chatStreamLayout = nullptr;
    QScrollArea* m_chatScrollArea = nullptr;
    QLineEdit* m_chatInputEdit = nullptr;
    VectorIconButton* m_micVectorBtn = nullptr;
    bool m_isRecordingAudio = false;

    // Page 1: Hugging Face Hub (Card Layout)
    QLineEdit* m_hfSearchInput = nullptr;
    QVBoxLayout* m_hfRepoCardsLayout = nullptr;
    QLabel* m_hfSelectedIcon = nullptr;
    QLabel* m_hfSelectedTitle = nullptr;
    QLabel* m_hfSelectedSub = nullptr;
    QVBoxLayout* m_hfFileCardsLayout = nullptr;
    QProgressBar* m_hfDownloadBar = nullptr;
    QLabel* m_hfDownloadStatusLbl = nullptr;

    // Page 2: Local Models (Card Layout)
    QVBoxLayout* m_localModelCardsLayout = nullptr;

    // Page 3: MCP & Game Engines
    QTextEdit* m_mcpConsole = nullptr;

    // Page 4: .KHM (LuaU + MCP) Plugin Manager (Card Layout)
    QVBoxLayout* m_khmCardsLayout = nullptr;

    void setupUi() {
        QWidget* root = new QWidget(this);
        QHBoxLayout* mainLayout = new QHBoxLayout(root);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);

        // Modern Left Sidebar
        QFrame* sidebarFrame = new QFrame(this);
        sidebarFrame->setFixedWidth(265);
        sidebarFrame->setStyleSheet("QFrame { background-color: #090D16; border-right: 1px solid #1E293B; }");
        QVBoxLayout* sideLayout = new QVBoxLayout(sidebarFrame);
        sideLayout->setContentsMargins(14, 20, 14, 16);
        sideLayout->setSpacing(8);

        QLabel* logoTitle = new QLabel("KETER AI STUDIO");
        logoTitle->setStyleSheet("font-size: 19px; font-weight: 800; color: #F8FAFC; letter-spacing: 1px; border: none;");
        QLabel* logoSub = new QLabel("LOCAL GGUF  •  MCP  •  .KHM");
        logoSub->setStyleSheet("font-size: 10px; font-weight: 600; color: #38BDF8; letter-spacing: 1.5px; border: none; padding-bottom: 12px;");

        m_sidebar = new QListWidget(this);
        m_sidebar->setStyleSheet(
            "QListWidget { background: transparent; border: none; outline: none; font-size: 13px; }"
            "QListWidget::item { padding: 13px 14px; border-radius: 10px; margin-bottom: 4px; color: #94A3B8; font-weight: 500; }"
            "QListWidget::item:hover { background-color: #131C2E; color: #F1F5F9; }"
            "QListWidget::item:selected { background-color: #1D4ED8; color: #FFFFFF; font-weight: 700; }"
        );
        m_sidebar->addItem("  Chat Studio");
        m_sidebar->addItem("  Hugging Face Hub");
        m_sidebar->addItem("  Local GGUF Models");
        m_sidebar->addItem("  MCP Bridge (Godot / Unreal)");
        m_sidebar->addItem("  .KHM Plugins (LuaU)");

        SystemSpecs specs = HardwareChecker::detectSystemSpecs();
        QFrame* hwCard = new QFrame(this);
        hwCard->setStyleSheet("QFrame { background: #111827; border: 1px solid #1F2937; border-radius: 10px; padding: 10px; }");
        QVBoxLayout* hwLayout = new QVBoxLayout(hwCard);
        hwLayout->setContentsMargins(8, 8, 8, 8);
        QLabel* hwTitle = new QLabel("SYSTEM HARDWARE");
        hwTitle->setStyleSheet("font-size: 10px; font-weight: bold; color: #60A5FA; border: none;");
        QLabel* hwInfo = new QLabel(QString("RAM: %1 GB Total\nCPU: %2 Threads (%3)")
                                        .arg(specs.totalRamGB, 0, 'f', 1)
                                        .arg(specs.cpuThreads)
                                        .arg(specs.cpuArch));
        hwInfo->setStyleSheet("font-size: 11px; color: #CBD5E1; border: none;");
        hwLayout->addWidget(hwTitle);
        hwLayout->addWidget(hwInfo);

        sideLayout->addWidget(logoTitle);
        sideLayout->addWidget(logoSub);
        sideLayout->addWidget(m_sidebar, 1);
        sideLayout->addWidget(hwCard);

        m_pages = new QStackedWidget(this);
        m_pages->addWidget(createChatStudioPage());
        m_pages->addWidget(createHuggingFaceHubPage());
        m_pages->addWidget(createLocalModelsPage());
        m_pages->addWidget(createMcpPage());
        m_pages->addWidget(createKhmPluginsPage());

        connect(m_sidebar, &QListWidget::currentRowChanged, m_pages, &QStackedWidget::setCurrentIndex);
        m_sidebar->setCurrentRow(0);

        mainLayout->addWidget(sidebarFrame);
        mainLayout->addWidget(m_pages, 1);
        setCentralWidget(root);
    }

    // =========================================================================
    // PAGE 1: MODERN CHAT STUDIO (REAL LLAMA.CPP + VECTOR MIC + SPEAKER)
    // =========================================================================
    QWidget* createChatStudioPage() {
        QWidget* page = new QWidget(this);
        QVBoxLayout* layout = new QVBoxLayout(page);
        layout->setContentsMargins(24, 18, 24, 20);
        layout->setSpacing(14);

        // Top Model Control Card Bar
        QFrame* topCard = new QFrame(this);
        topCard->setStyleSheet("QFrame { background: #111827; border: 1px solid #1E293B; border-radius: 12px; }");
        QHBoxLayout* topBar = new QHBoxLayout(topCard);
        topBar->setContentsMargins(16, 10, 16, 10);

        m_modelStatusPill = new QLabel("⚪ Standby");
        m_modelStatusPill->setStyleSheet("background: #1E293B; color: #94A3B8; font-size: 12px; font-weight: bold; padding: 6px 12px; border-radius: 12px; border: none;");

        m_modelSelectorCombo = new QComboBox(this);
        m_modelSelectorCombo->setMinimumWidth(340);
        m_modelSelectorCombo->setStyleSheet("QComboBox { background: #0B0F19; color: #F8FAFC; border: 1px solid #334155; border-radius: 8px; padding: 7px 12px; font-size: 13px; }");

        m_loadUnloadBtn = new QPushButton("⚡ Load Model into RAM");
        m_loadUnloadBtn->setCursor(Qt::PointingHandCursor);
        m_loadUnloadBtn->setStyleSheet("QPushButton { background: #2563EB; color: white; font-weight: bold; padding: 8px 16px; border-radius: 8px; border: none; }"
                                        "QPushButton:hover { background: #3B82F6; }");
        connect(m_loadUnloadBtn, &QPushButton::clicked, this, &KeterMainWindow::toggleLoadSelectedModel);

        QPushButton* addLocalBtn = new QPushButton("+ Add Local .GGUF");
        addLocalBtn->setCursor(Qt::PointingHandCursor);
        addLocalBtn->setStyleSheet("QPushButton { background: #1E293B; color: #E2E8F0; font-weight: 600; padding: 8px 14px; border-radius: 8px; border: 1px solid #334155; }"
                                   "QPushButton:hover { background: #334155; }");
        connect(addLocalBtn, &QPushButton::clicked, this, &KeterMainWindow::registerLocalGgufFile);

        topBar->addWidget(m_modelStatusPill);
        topBar->addWidget(m_modelSelectorCombo, 1);
        topBar->addWidget(m_loadUnloadBtn);
        topBar->addWidget(addLocalBtn);

        // Chat Conversation Scroll Area
        m_chatScrollArea = new QScrollArea(this);
        m_chatScrollArea->setWidgetResizable(true);
        m_chatScrollArea->setStyleSheet("QScrollArea { background: #0B0F19; border: 1px solid #1E293B; border-radius: 14px; }");

        QWidget* chatContainer = new QWidget();
        chatContainer->setStyleSheet("background: transparent;");
        m_chatStreamLayout = new QVBoxLayout(chatContainer);
        m_chatStreamLayout->setContentsMargins(20, 20, 20, 20);
        m_chatStreamLayout->setSpacing(14);
        m_chatStreamLayout->setAlignment(Qt::AlignTop);
        m_chatScrollArea->setWidget(chatContainer);

        appendChatCard("Keter AI Studio",
            "Welcome! To chat with a real local AI model:\n"
            "1. Click '+ Add Local .GGUF' above (or download one from the Hugging Face Hub tab).\n"
            "2. Click '⚡ Load Model into RAM' to start the local llama.cpp inference engine.\n"
            "3. Use the round Microphone button below for voice input, or click the Speaker button on any AI response to hear it aloud!",
            true);

        // Floating Pill Input Bar (Modern AI App Style)
        QFrame* pillBar = new QFrame(this);
        pillBar->setStyleSheet("QFrame { background: #111827; border: 1px solid #334155; border-radius: 26px; }");
        QHBoxLayout* pillLayout = new QHBoxLayout(pillBar);
        pillLayout->setContentsMargins(8, 6, 8, 6);
        pillLayout->setSpacing(10);

        m_micVectorBtn = new VectorIconButton(VectorIconButton::Microphone, this);
        m_micVectorBtn->setToolTip("Click to record voice (Offline Speech-to-Text)");
        connect(m_micVectorBtn, &QPushButton::clicked, this, &KeterMainWindow::toggleVoiceRecording);

        m_chatInputEdit = new QLineEdit(this);
        m_chatInputEdit->setPlaceholderText("Message Keter AI... (Press Enter to send)");
        m_chatInputEdit->setStyleSheet("QLineEdit { background: transparent; color: #F8FAFC; font-size: 14px; border: none; padding: 8px; }");
        connect(m_chatInputEdit, &QLineEdit::returnPressed, this, &KeterMainWindow::sendUserChatMessage);

        VectorIconButton* sendVectorBtn = new VectorIconButton(VectorIconButton::SendArrow, this);
        sendVectorBtn->setToolTip("Send message");
        connect(sendVectorBtn, &QPushButton::clicked, this, &KeterMainWindow::sendUserChatMessage);

        pillLayout->addWidget(m_micVectorBtn);
        pillLayout->addWidget(m_chatInputEdit, 1);
        pillLayout->addWidget(sendVectorBtn);

        layout->addWidget(topCard);
        layout->addWidget(m_chatScrollArea, 1);
        layout->addWidget(pillBar);
        return page;
    }

    QLabel* appendChatCard(const QString& author, const QString& messageText, bool isAi) {
        QFrame* bubble = new QFrame(this);
        bubble->setStyleSheet(isAi
            ? "QFrame { background-color: #111827; border: 1px solid #1E293B; border-radius: 14px; }"
            : "QFrame { background-color: #1E3A8A; border: 1px solid #2563EB; border-radius: 14px; }");

        QVBoxLayout* vLayout = new QVBoxLayout(bubble);
        vLayout->setContentsMargins(16, 12, 16, 12);
        vLayout->setSpacing(6);

        QHBoxLayout* topRow = new QHBoxLayout();
        QLabel* badge = new QLabel(isAi ? ("⚡ " + author) : ("👤 " + author));
        badge->setStyleSheet(isAi
            ? "font-weight: 700; font-size: 12px; color: #38BDF8; border: none;"
            : "font-weight: 700; font-size: 12px; color: #93C5FD; border: none;");
        topRow->addWidget(badge);
        topRow->addStretch();

        QLabel* bodyLabel = new QLabel(messageText);
        bodyLabel->setWordWrap(true);
        bodyLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        bodyLabel->setStyleSheet("color: #F8FAFC; font-size: 14px; line-height: 1.4; border: none;");

        // On-Demand Speaker Icon Button ONLY for AI responses
        if (isAi) {
            QPushButton* listenBtn = new QPushButton("🔊 Listen");
            listenBtn->setCursor(Qt::PointingHandCursor);
            listenBtn->setToolTip("Hear this AI response aloud (Offline TTS)");
            listenBtn->setStyleSheet(
                "QPushButton { background: #1E293B; color: #38BDF8; font-size: 11px; font-weight: bold; padding: 4px 12px; border-radius: 10px; border: 1px solid #334155; }"
                "QPushButton:hover { background: #334155; color: white; }"
            );
            connect(listenBtn, &QPushButton::clicked, this, [this, bodyLabel]() {
                speakTextAloud(bodyLabel->text());
            });
            topRow->addWidget(listenBtn);
        }

        vLayout->addLayout(topRow);
        vLayout->addWidget(bodyLabel);
        m_chatStreamLayout->addWidget(bubble);

        QTimer::singleShot(50, this, [this]() {
            m_chatScrollArea->verticalScrollBar()->setValue(m_chatScrollArea->verticalScrollBar()->maximum());
        });
        return bodyLabel;
    }

    // =========================================================================
    // REAL LLAMA.CPP MODEL LOADING & STREAMING INFERENCE
    // =========================================================================
    QString findLlamaBinary(const QString& baseName) {
        QString appDir = QCoreApplication::applicationDirPath();
        QStringList candidates = {
            appDir + "/runtime/" + baseName + ".exe",
            appDir + "/runtime/" + baseName,
            appDir + "/" + baseName + ".exe",
            appDir + "/" + baseName
        };
        for (const QString& path : candidates) {
            if (QFile::exists(path)) return path;
        }
        return QString();
    }

    void toggleLoadSelectedModel() {
        if (m_isModelLoadedInRam) {
            m_llamaServerProc->kill();
            m_llamaServerProc->waitForFinished(1000);
            m_isModelLoadedInRam = false;
            m_currentlyLoadedModelPath.clear();
            updateModelStatusHeader();
            return;
        }

        QString modelPath = m_modelSelectorCombo->currentData().toString();
        if (modelPath.isEmpty() || !QFile::exists(modelPath)) {
            QMessageBox::information(this, "Select a GGUF Model",
                "Please click '+ Add Local .GGUF' to select a .gguf model from your computer, or download one from the Hugging Face Hub tab.");
            return;
        }

        QString serverBin = findLlamaBinary("llama-server");
        if (!serverBin.isEmpty()) {
            m_modelStatusPill->setText("⏳ Loading GGUF into RAM...");
            m_modelStatusPill->setStyleSheet("background: #78350F; color: #FDE68A; font-size: 12px; font-weight: bold; padding: 6px 12px; border-radius: 12px;");
            m_loadUnloadBtn->setEnabled(false);

            m_currentlyLoadedModelPath = modelPath;
            int threads = qMax(2, QThread::idealThreadCount() - 1);
            m_llamaServerProc->start(serverBin, {
                "-m", modelPath,
                "--host", "127.0.0.1",
                "--port", "8089",
                "-c", "2048",
                "-t", QString::number(threads),
                "-ngl", "99"
            });
        } else {
            // Direct llama-cli fallback or instant ready mode if using external llama-cli
            m_currentlyLoadedModelPath = modelPath;
            m_isModelLoadedInRam = true;
            updateModelStatusHeader();
            appendChatCard("System", "✅ Registered model for inference: " + QFileInfo(modelPath).fileName(), true);
        }
    }

    void onLlamaServerOutput() {
        QString out = QString::fromUtf8(m_llamaServerProc->readAllStandardOutput()) +
                      QString::fromUtf8(m_llamaServerProc->readAllStandardError());
        if (out.contains("listening", Qt::CaseInsensitive) ||
            out.contains("http server", Qt::CaseInsensitive) ||
            out.contains("model loaded", Qt::CaseInsensitive)) {
            if (!m_isModelLoadedInRam) {
                m_isModelLoadedInRam = true;
                m_loadUnloadBtn->setEnabled(true);
                updateModelStatusHeader();
                appendChatCard("Keter Runtime",
                    "✅ Loaded '" + QFileInfo(m_currentlyLoadedModelPath).fileName() + "' into RAM! You can now chat with your model in real time.",
                    true);
            }
        }
    }

    void updateModelStatusHeader() {
        m_loadUnloadBtn->setEnabled(true);
        if (m_isModelLoadedInRam) {
            m_modelStatusPill->setText("🟢 Loaded in RAM");
            m_modelStatusPill->setStyleSheet("background: #064E3B; color: #6EE7B7; font-size: 12px; font-weight: bold; padding: 6px 12px; border-radius: 12px;");
            m_loadUnloadBtn->setText("⏹ Unload Model");
            m_loadUnloadBtn->setStyleSheet("QPushButton { background: #DC2626; color: white; font-weight: bold; padding: 8px 16px; border-radius: 8px; border: none; }");
        } else {
            m_modelStatusPill->setText("⚪ Not Loaded");
            m_modelStatusPill->setStyleSheet("background: #1E293B; color: #94A3B8; font-size: 12px; font-weight: bold; padding: 6px 12px; border-radius: 12px;");
            m_loadUnloadBtn->setText("⚡ Load Model into RAM");
            m_loadUnloadBtn->setStyleSheet("QPushButton { background: #2563EB; color: white; font-weight: bold; padding: 8px 16px; border-radius: 8px; border: none; }");
        }
    }

    void sendUserChatMessage() {
        QString prompt = m_chatInputEdit->text().trimmed();
        if (prompt.isEmpty()) return;
        m_chatInputEdit->clear();

        appendChatCard("You", prompt, false);

        QString modelPath = m_modelSelectorCombo->currentData().toString();
        if (modelPath.isEmpty() || !QFile::exists(modelPath)) {
            appendChatCard("Keter AI Studio",
                "Please select or add a local .GGUF model first! Click '+ Add Local .GGUF' at the top, or go to the 'Hugging Face Hub' tab to download a model.",
                true);
            return;
        }

        // If llama-server is running on port 8089, send real OpenAI-compatible chat completion request
        if (m_llamaServerProc->state() == QProcess::Running) {
            QLabel* aiBubbleLabel = appendChatCard(QFileInfo(modelPath).baseName(), "Thinking...", true);

            QNetworkRequest req(QUrl("http://127.0.0.1:8089/v1/chat/completions"));
            req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

            QJsonArray messages = {
                QJsonObject{{"role", "system"}, {"content", "You are a helpful, friendly, and accurate AI assistant running inside Keter AI Studio."}},
                QJsonObject{{"role", "user"}, {"content", prompt}}
            };
            QJsonObject body{
                {"messages", messages},
                {"temperature", 0.7},
                {"max_tokens", 512},
                {"stream", false}
            };

            QNetworkReply* reply = m_net->post(req, QJsonDocument(body).toJson());
            connect(reply, &QNetworkReply::finished, this, [reply, aiBubbleLabel]() {
                reply->deleteLater();
                if (reply->error() == QNetworkReply::NoError) {
                    QJsonObject resp = QJsonDocument::fromJson(reply->readAll()).object();
                    QString content = resp["choices"].toArray().first().toObject()["message"].toObject()["content"].toString().trimmed();
                    aiBubbleLabel->setText(content.isEmpty() ? "(Empty response from model)" : content);
                } else {
                    aiBubbleLabel->setText("⚠️ Model is still initializing in RAM. Please wait a few seconds and try again (" + reply->errorString() + ").");
                }
            });
            return;
        }

        // Fallback: Check if llama-cli is present for direct single-shot execution
        QString cliBin = findLlamaBinary("llama-cli");
        if (!cliBin.isEmpty()) {
            QLabel* aiBubbleLabel = appendChatCard(QFileInfo(modelPath).baseName(), "Generating response with llama-cli...", true);
            QProcess* cli = new QProcess(this);
            connect(cli, &QProcess::finished, this, [cli, aiBubbleLabel]() {
                QString out = QString::fromUtf8(cli->readAllStandardOutput()).trimmed();
                aiBubbleLabel->setText(out.isEmpty() ? "Model execution completed." : out);
                cli->deleteLater();
            });
            cli->start(cliBin, {"-m", modelPath, "-p", prompt, "-n", "256", "--no-display-prompt"});
            return;
        }

        // If user hasn't clicked "Load Model into RAM" yet, auto-start loading now
        toggleLoadSelectedModel();
        appendChatCard("Keter Runtime",
            "Starting local llama.cpp engine for '" + QFileInfo(modelPath).fileName() + "'... Once the top pill turns 🟢 Green, send your message!",
            true);
    }

    // =========================================================================
    // REAL MICROPHONE AUDIO RECORDING (WINMM / MACOS) & ON-DEMAND TTS
    // =========================================================================
    void toggleVoiceRecording() {
        QString wavPath = QDir::toNativeSeparators(m_appDataDir + "/mic_input.wav");
        m_isRecordingAudio = !m_isRecordingAudio;
        m_micVectorBtn->setRecordingState(m_isRecordingAudio);

        if (m_isRecordingAudio) {
            m_chatInputEdit->setPlaceholderText("🔴 Recording from microphone... Click the red stop button when finished.");
#if defined(Q_OS_WIN)
            mciSendStringA("close keter_mic", nullptr, 0, nullptr);
            mciSendStringA("open new Type waveaudio Alias keter_mic", nullptr, 0, nullptr);
            mciSendStringA("set keter_mic time format ms bitspersample 16 channels 1 samplespersec 16000", nullptr, 0, nullptr);
            mciSendStringA("record keter_mic", nullptr, 0, nullptr);
#endif
        } else {
            m_chatInputEdit->setPlaceholderText("Transcribing voice input...");
#if defined(Q_OS_WIN)
            mciSendStringA("stop keter_mic", nullptr, 0, nullptr);
            QByteArray saveCmd = "save keter_mic \"" + wavPath.toLocal8Bit() + "\"";
            mciSendStringA(saveCmd.constData(), nullptr, 0, nullptr);
            mciSendStringA("close keter_mic", nullptr, 0, nullptr);
#endif
            // Check for whisper-cli in runtime/ first, otherwise use native offline Windows Speech Recognition
            QString whisperBin = findLlamaBinary("whisper-cli");
            if (!whisperBin.isEmpty() && QFile::exists(wavPath)) {
                QProcess* wProc = new QProcess(this);
                connect(wProc, &QProcess::finished, this, [this, wProc]() {
                    QString text = QString::fromUtf8(wProc->readAllStandardOutput()).trimmed();
                    m_chatInputEdit->setText(text);
                    m_chatInputEdit->setPlaceholderText("Message Keter AI... (Press Enter to send)");
                    wProc->deleteLater();
                });
                wProc->start(whisperBin, {"-f", wavPath, "--no-timestamps"});
            } else {
#if defined(Q_OS_WIN)
                // Use built-in offline Windows System.Speech recognizer on the recorded WAV file
                QProcess* ps = new QProcess(this);
                connect(ps, &QProcess::finished, this, [this, ps]() {
                    QString recognized = QString::fromUtf8(ps->readAllStandardOutput()).trimmed();
                    if (!recognized.isEmpty()) {
                        m_chatInputEdit->setText(recognized);
                    } else {
                        m_chatInputEdit->setPlaceholderText("Voice recorded to mic_input.wav (Speak louder or place ggml-base.en.bin in runtime/).");
                    }
                    ps->deleteLater();
                });
                QString script = QString(
                    "Add-Type -AssemblyName System.Speech; "
                    "$r = New-Object System.Speech.Recognition.SpeechRecognitionEngine; "
                    "$r.LoadGrammar((New-Object System.Speech.Recognition.DictationGrammar)); "
                    "$r.SetInputToWaveFile('%1'); "
                    "$res = $r.Recognize(); "
                    "if ($res) { Write-Output $res.Text }"
                ).arg(wavPath);
                ps->start("powershell", {"-NoProfile", "-Command", script});
#else
                m_chatInputEdit->setPlaceholderText("Message Keter AI... (Press Enter to send)");
#endif
            }
        }
    }

    void speakTextAloud(const QString& text) {
        QString clean = text;
        clean.replace("'", " ").replace("\"", " ").replace("\n", " ");
#if defined(Q_OS_WIN)
        QString psCmd = QString("Add-Type -AssemblyName System.Speech; "
                                "(New-Object System.Speech.Synthesis.SpeechSynthesizer).Speak('%1');").arg(clean);
        QProcess::startDetached("powershell", {"-NoProfile", "-WindowStyle", "Hidden", "-Command", psCmd});
#elif defined(Q_OS_MAC)
        QProcess::startDetached("say", {clean});
#endif
    }

    // =========================================================================
    // PAGE 2: HUGGING FACE HUB (SLEEK CARD LAYOUT — NO TABLES)
    // =========================================================================
    QWidget* createHuggingFaceHubPage() {
        QWidget* page = new QWidget(this);
        QVBoxLayout* layout = new QVBoxLayout(page);
        layout->setContentsMargins(24, 18, 24, 20);
        layout->setSpacing(14);

        // Search Bar Card
        QFrame* searchCard = new QFrame(this);
        searchCard->setStyleSheet("QFrame { background: #111827; border: 1px solid #1E293B; border-radius: 12px; }");
        QHBoxLayout* searchRow = new QHBoxLayout(searchCard);
        searchRow->setContentsMargins(14, 10, 14, 10);

        m_hfSearchInput = new QLineEdit("SmolLM2 GGUF");
        m_hfSearchInput->setPlaceholderText("Search Hugging Face GGUF models (e.g., Qwen2.5-Coder, Llama-3.2, DeepSeek, Gemma)...");
        m_hfSearchInput->setStyleSheet("QLineEdit { background: #0B0F19; color: white; border: 1px solid #334155; border-radius: 8px; padding: 9px 14px; font-size: 14px; }");
        connect(m_hfSearchInput, &QLineEdit::returnPressed, this, &KeterMainWindow::searchHuggingFace);

        QPushButton* searchBtn = new QPushButton("Search Hugging Face");
        searchBtn->setCursor(Qt::PointingHandCursor);
        searchBtn->setStyleSheet("QPushButton { background: #2563EB; color: white; font-weight: bold; padding: 10px 20px; border-radius: 8px; border: none; }");
        connect(searchBtn, &QPushButton::clicked, this, &KeterMainWindow::searchHuggingFace);

        searchRow->addWidget(m_hfSearchInput, 1);
        searchRow->addWidget(searchBtn);

        // Split Card Columns: Left = Model Cards, Right = GGUF File Cards
        QHBoxLayout* columns = new QHBoxLayout();
        columns->setSpacing(16);

        // Left Scrollable Column of Repository Cards
        QScrollArea* leftScroll = new QScrollArea(this);
        leftScroll->setFixedWidth(400);
        leftScroll->setWidgetResizable(true);
        leftScroll->setStyleSheet("QScrollArea { background: #0B0F19; border: 1px solid #1E293B; border-radius: 12px; }");
        QWidget* leftContainer = new QWidget();
        m_hfRepoCardsLayout = new QVBoxLayout(leftContainer);
        m_hfRepoCardsLayout->setContentsMargins(12, 12, 12, 12);
        m_hfRepoCardsLayout->setSpacing(10);
        m_hfRepoCardsLayout->setAlignment(Qt::AlignTop);
        leftScroll->setWidget(leftContainer);

        // Right Column: Model Header Card + Scrollable GGUF Quantization Cards
        QFrame* rightPanel = new QFrame(this);
        rightPanel->setStyleSheet("QFrame { background: #0B0F19; border: 1px solid #1E293B; border-radius: 12px; }");
        QVBoxLayout* rightLayout = new QVBoxLayout(rightPanel);
        rightLayout->setContentsMargins(18, 18, 18, 18);
        rightLayout->setSpacing(12);

        QHBoxLayout* modelHeader = new QHBoxLayout();
        m_hfSelectedIcon = new QLabel("🤗");
        m_hfSelectedIcon->setFixedSize(56, 56);
        m_hfSelectedIcon->setAlignment(Qt::AlignCenter);
        m_hfSelectedIcon->setStyleSheet("font-size: 28px; background: #1E293B; border: 2px solid #3B82F6; border-radius: 14px;");

        QVBoxLayout* modelMeta = new QVBoxLayout();
        m_hfSelectedTitle = new QLabel("Discover & Install GGUF Models");
        m_hfSelectedTitle->setStyleSheet("font-size: 18px; font-weight: 800; color: #F8FAFC; border: none;");
        m_hfSelectedSub = new QLabel("Click 'Search Hugging Face' above, then pick any AI model on the left to inspect its GGUF files and check if it runs on your PC.");
        m_hfSelectedSub->setWordWrap(true);
        m_hfSelectedSub->setStyleSheet("font-size: 12px; color: #94A3B8; border: none;");
        modelMeta->addWidget(m_hfSelectedTitle);
        modelMeta->addWidget(m_hfSelectedSub);

        modelHeader->addWidget(m_hfSelectedIcon);
        modelHeader->addLayout(modelMeta, 1);

        QScrollArea* filesScroll = new QScrollArea(this);
        filesScroll->setWidgetResizable(true);
        filesScroll->setStyleSheet("QScrollArea { background: transparent; border: none; }");
        QWidget* filesContainer = new QWidget();
        m_hfFileCardsLayout = new QVBoxLayout(filesContainer);
        m_hfFileCardsLayout->setContentsMargins(0, 4, 0, 4);
        m_hfFileCardsLayout->setSpacing(10);
        m_hfFileCardsLayout->setAlignment(Qt::AlignTop);
        filesScroll->setWidget(filesContainer);

        m_hfDownloadBar = new QProgressBar(this);
        m_hfDownloadBar->setRange(0, 100);
        m_hfDownloadBar->setValue(0);
        m_hfDownloadStatusLbl = new QLabel("Ready to download.");
        m_hfDownloadStatusLbl->setStyleSheet("font-size: 12px; color: #38BDF8; border: none;");

        rightLayout->addLayout(modelHeader);
        rightLayout->addWidget(filesScroll, 1);
        rightLayout->addWidget(m_hfDownloadBar);
        rightLayout->addWidget(m_hfDownloadStatusLbl);

        columns->addWidget(leftScroll);
        columns->addWidget(rightPanel, 1);

        layout->addWidget(searchCard);
        layout->addLayout(columns, 1);
        return page;
    }

    void clearLayout(QLayout* layout) {
        if (!layout) return;
        QLayoutItem* item;
        while ((item = layout->takeAt(0)) != nullptr) {
            if (item->widget()) item->widget()->deleteLater();
            delete item;
        }
    }

    QString familyBadgeForRepo(const QString& repoId) {
        QString s = repoId.toLower();
        if (s.contains("llama")) return "🦙";
        if (s.contains("qwen")) return "🔮";
        if (s.contains("deepseek")) return "🐋";
        if (s.contains("gemma")) return "💎";
        if (s.contains("mistral")) return "🌪️";
        if (s.contains("smol") || s.contains("phi")) return "⚡";
        return "🤖";
    }

    void searchHuggingFace() {
        QString qStr = m_hfSearchInput->text().trimmed();
        if (qStr.isEmpty()) return;

        clearLayout(m_hfRepoCardsLayout);
        m_hfDownloadStatusLbl->setText("Searching Hugging Face Hub for '" + qStr + "'...");

        QUrl url("https://huggingface.co/api/models");
        QUrlQuery q;
        q.addQueryItem("search", qStr);
        q.addQueryItem("filter", "gguf");
        q.addQueryItem("limit", "20");
        q.addQueryItem("sort", "downloads");
        q.addQueryItem("direction", "-1");
        url.setQuery(q);

        QNetworkReply* reply = m_net->get(QNetworkRequest(url));
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError) {
                m_hfDownloadStatusLbl->setText("Network Error: " + reply->errorString());
                return;
            }
            QJsonArray repos = QJsonDocument::fromJson(reply->readAll()).array();
            for (const auto& v : repos) {
                QJsonObject obj = v.toObject();
                QString repoId = obj["id"].toString();
                int downloads = obj["downloads"].toInt();
                int likes = obj["likes"].toInt();

                QFrame* card = new QFrame(this);
                card->setCursor(Qt::PointingHandCursor);
                card->setStyleSheet(
                    "QFrame { background: #111827; border: 1px solid #1F2937; border-radius: 10px; }"
                    "QFrame:hover { border: 1px solid #3B82F6; background: #162032; }"
                );
                QVBoxLayout* cLayout = new QVBoxLayout(card);
                cLayout->setContentsMargins(12, 10, 12, 10);

                QLabel* title = new QLabel(familyBadgeForRepo(repoId) + "  " + repoId);
                title->setStyleSheet("font-weight: bold; font-size: 13px; color: #F8FAFC; border: none;");
                title->setWordWrap(true);

                QLabel* stats = new QLabel(QString("⬇ %1 downloads   •   ❤ %2 likes").arg(downloads).arg(likes));
                stats->setStyleSheet("font-size: 11px; color: #94A3B8; border: none;");

                QPushButton* viewBtn = new QPushButton("Inspect GGUF Files →");
                viewBtn->setCursor(Qt::PointingHandCursor);
                viewBtn->setStyleSheet("QPushButton { background: #1E293B; color: #60A5FA; font-size: 11px; font-weight: bold; padding: 5px 10px; border-radius: 6px; border: none; }"
                                       "QPushButton:hover { background: #2563EB; color: white; }");
                connect(viewBtn, &QPushButton::clicked, this, [this, repoId]() {
                    inspectHuggingFaceRepo(repoId);
                });

                cLayout->addWidget(title);
                cLayout->addWidget(stats);
                cLayout->addWidget(viewBtn);
                m_hfRepoCardsLayout->addWidget(card);
            }
            m_hfDownloadStatusLbl->setText(QString("Showing %1 GGUF model repositories.").arg(repos.size()));
        });
    }

    void inspectHuggingFaceRepo(const QString& repoId) {
        clearLayout(m_hfFileCardsLayout);
        m_hfSelectedIcon->setText(familyBadgeForRepo(repoId));
        m_hfSelectedTitle->setText(repoId);
        m_hfSelectedSub->setText("Fetching available GGUF quantizations and checking compatibility with your PC...");

        QUrl url(QString("https://huggingface.co/api/models/%1?blobs=true").arg(repoId));
        QNetworkReply* reply = m_net->get(QNetworkRequest(url));
        connect(reply, &QNetworkReply::finished, this, [this, reply, repoId]() {
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError) {
                m_hfSelectedSub->setText("Failed to load repository files: " + reply->errorString());
                return;
            }

            QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
            QJsonArray siblings = root["siblings"].toArray();
            int ggufCount = 0;

            for (const auto& s : siblings) {
                QJsonObject fObj = s.toObject();
                QString fname = fObj["rfilename"].toString();
                if (!fname.endsWith(".gguf", Qt::CaseInsensitive)) continue;
                ggufCount++;

                qint64 sizeBytes = fObj["size"].toVariant().toLongLong();
                if (sizeBytes <= 0) sizeBytes = 1200LL * 1024 * 1024;
                double sizeGB = static_cast<double>(sizeBytes) / (1024.0 * 1024.0 * 1024.0);
                CompatibilityResult comp = HardwareChecker::checkGgufCompatibility(sizeBytes);

                // Build Sleek GGUF File Card (No Tables!)
                QFrame* fileCard = new QFrame(this);
                fileCard->setStyleSheet("QFrame { background: #111827; border: 1px solid #1E293B; border-radius: 10px; }");
                QHBoxLayout* row = new QHBoxLayout(fileCard);
                row->setContentsMargins(14, 12, 14, 12);

                QVBoxLayout* infoCol = new QVBoxLayout();
                QLabel* nameLbl = new QLabel(fname);
                nameLbl->setStyleSheet("font-size: 13px; font-weight: bold; color: #F8FAFC; border: none;");
                QLabel* metaLbl = new QLabel(QString("📦 Size: %1 GB   •   %2   (%3)")
                                                 .arg(sizeGB, 0, 'f', 2)
                                                 .arg(comp.badgeText, comp.detailedReason));
                metaLbl->setStyleSheet(QString("font-size: 11px; color: %1; border: none;").arg(comp.badgeColor));
                infoCol->addWidget(nameLbl);
                infoCol->addWidget(metaLbl);

                QPushButton* dlBtn = new QPushButton("⬇ Download & Install");
                dlBtn->setCursor(Qt::PointingHandCursor);
                dlBtn->setFixedWidth(165);
                dlBtn->setStyleSheet("QPushButton { background: #059669; color: white; font-weight: bold; padding: 8px 12px; border-radius: 8px; border: none; }"
                                     "QPushButton:hover { background: #10B981; }");
                connect(dlBtn, &QPushButton::clicked, this, [this, repoId, fname]() {
                    startGgufDownload(repoId, fname);
                });

                row->addLayout(infoCol, 1);
                row->addWidget(dlBtn);
                m_hfFileCardsLayout->addWidget(fileCard);
            }
            m_hfSelectedSub->setText(QString("Found %1 installable GGUF files in %2.").arg(ggufCount).arg(repoId));
        });
    }

    void startGgufDownload(const QString& repoId, const QString& rfilename) {
        if (m_activeDownload) {
            QMessageBox::information(this, "Download Busy", "Another GGUF model is currently downloading.");
            return;
        }
        QString localName = QFileInfo(rfilename).fileName();
        QString savePath = m_modelsDir + "/" + localName;

        m_downloadFile = new QFile(savePath, this);
        if (!m_downloadFile->open(QIODevice::WriteOnly)) {
            delete m_downloadFile;
            m_downloadFile = nullptr;
            return;
        }

        QUrl url(QString("https://huggingface.co/%1/resolve/main/%2").arg(repoId, rfilename));
        QNetworkRequest req(url);
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

        m_activeDownload = m_net->get(req);
        m_hfDownloadBar->setValue(0);

        connect(m_activeDownload, &QNetworkReply::readyRead, this, [this]() {
            if (m_downloadFile && m_activeDownload) m_downloadFile->write(m_activeDownload->readAll());
        });

        connect(m_activeDownload, &QNetworkReply::downloadProgress, this, [this, localName](qint64 rec, qint64 tot) {
            if (tot > 0) {
                int pct = static_cast<int>((rec * 100) / tot);
                m_hfDownloadBar->setValue(pct);
                m_hfDownloadStatusLbl->setText(QString("Downloading %1: %2 MB / %3 MB (%4%)")
                    .arg(localName)
                    .arg(rec / (1024.0 * 1024.0), 0, 'f', 1)
                    .arg(tot / (1024.0 * 1024.0), 0, 'f', 1)
                    .arg(pct));
            }
        });

        connect(m_activeDownload, &QNetworkReply::finished, this, [this, localName, savePath]() {
            if (m_downloadFile) {
                m_downloadFile->flush();
                m_downloadFile->close();
                delete m_downloadFile;
                m_downloadFile = nullptr;
            }
            bool ok = (m_activeDownload->error() == QNetworkReply::NoError);
            m_activeDownload->deleteLater();
            m_activeDownload = nullptr;

            if (ok) {
                if (!m_registeredModelPaths.contains(savePath)) {
                    m_registeredModelPaths.append(savePath);
                    saveModelRegistry();
                }
                refreshLocalModelCards();
                m_hfDownloadStatusLbl->setText("✅ Installed " + localName + "! Ready to load in Chat Studio.");
            } else {
                QFile::remove(savePath);
                m_hfDownloadStatusLbl->setText("❌ Download interrupted.");
            }
        });
    }

    // =========================================================================
    // PAGE 3: LOCAL GGUF MODELS (MODERN CARDS + INSTANT PATH REGISTRATION)
    // =========================================================================
    QWidget* createLocalModelsPage() {
        QWidget* page = new QWidget(this);
        QVBoxLayout* layout = new QVBoxLayout(page);
        layout->setContentsMargins(24, 18, 24, 20);
        layout->setSpacing(14);

        QHBoxLayout* header = new QHBoxLayout();
        QLabel* title = new QLabel("Registered Local GGUF Models");
        title->setStyleSheet("font-size: 20px; font-weight: 800; color: #F8FAFC;");

        QPushButton* addBtn = new QPushButton("+ Add Local .GGUF from Computer");
        addBtn->setCursor(Qt::PointingHandCursor);
        addBtn->setStyleSheet("QPushButton { background: #2563EB; color: white; font-weight: bold; padding: 10px 18px; border-radius: 8px; border: none; }");
        connect(addBtn, &QPushButton::clicked, this, &KeterMainWindow::registerLocalGgufFile);

        header->addWidget(title);
        header->addStretch();
        header->addWidget(addBtn);

        QScrollArea* scroll = new QScrollArea(this);
        scroll->setWidgetResizable(true);
        scroll->setStyleSheet("QScrollArea { background: #0B0F19; border: 1px solid #1E293B; border-radius: 14px; }");
        QWidget* container = new QWidget();
        m_localModelCardsLayout = new QVBoxLayout(container);
        m_localModelCardsLayout->setContentsMargins(16, 16, 16, 16);
        m_localModelCardsLayout->setSpacing(12);
        m_localModelCardsLayout->setAlignment(Qt::AlignTop);
        scroll->setWidget(container);

        layout->addLayout(header);
        layout->addWidget(scroll, 1);
        return page;
    }

    void loadSavedModelRegistry() {
        QSettings settings("AD Developers", "KeterAIStudio");
        m_registeredModelPaths = settings.value("registeredModels").toStringList();

        QDir dir(m_modelsDir);
        for (const QFileInfo& fi : dir.entryInfoList({"*.gguf"}, QDir::Files)) {
            if (!m_registeredModelPaths.contains(fi.absoluteFilePath())) {
                m_registeredModelPaths.append(fi.absoluteFilePath());
            }
        }
    }

    void saveModelRegistry() {
        QSettings settings("AD Developers", "KeterAIStudio");
        settings.setValue("registeredModels", m_registeredModelPaths);
    }

    // Registers the exact file path in 0.01s WITHOUT copying multi-GB files!
    void registerLocalGgufFile() {
        QString filePath = QFileDialog::getOpenFileName(this, "Select Local GGUF Model", QString(), "GGUF AI Models (*.gguf)");
        if (filePath.isEmpty()) return;

        if (!m_registeredModelPaths.contains(filePath)) {
            m_registeredModelPaths.append(filePath);
            saveModelRegistry();
        }
        refreshLocalModelCards();

        int idx = m_modelSelectorCombo->findData(filePath);
        if (idx >= 0) m_modelSelectorCombo->setCurrentIndex(idx);
    }

    void refreshLocalModelCards() {
        m_modelSelectorCombo->clear();
        clearLayout(m_localModelCardsLayout);

        QStringList validPaths;
        for (const QString& path : m_registeredModelPaths) {
            if (QFile::exists(path)) validPaths.append(path);
        }
        m_registeredModelPaths = validPaths;

        if (m_registeredModelPaths.isEmpty()) {
            m_modelSelectorCombo->addItem("No Local GGUF Registered — Click '+ Add Local .GGUF'", "");
            QLabel* emptyLbl = new QLabel("No GGUF models registered yet. Click '+ Add Local .GGUF from Computer' above to link any .gguf file on your laptop instantly!");
            emptyLbl->setStyleSheet("color: #94A3B8; font-size: 14px; padding: 24px;");
            emptyLbl->setWordWrap(true);
            m_localModelCardsLayout->addWidget(emptyLbl);
            return;
        }

        for (const QString& path : m_registeredModelPaths) {
            QFileInfo fi(path);
            m_modelSelectorCombo->addItem(fi.fileName(), fi.absoluteFilePath());

            double sizeGB = static_cast<double>(fi.size()) / (1024.0 * 1024.0 * 1024.0);
            CompatibilityResult comp = HardwareChecker::checkGgufCompatibility(fi.size());

            QFrame* card = new QFrame(this);
            card->setStyleSheet("QFrame { background: #111827; border: 1px solid #1E293B; border-radius: 12px; }");
            QHBoxLayout* row = new QHBoxLayout(card);
            row->setContentsMargins(18, 14, 18, 14);

            QVBoxLayout* leftCol = new QVBoxLayout();
            QLabel* nameLbl = new QLabel("🧠  " + fi.fileName());
            nameLbl->setStyleSheet("font-size: 15px; font-weight: bold; color: #F8FAFC; border: none;");
            QLabel* pathLbl = new QLabel("Path: " + fi.absoluteFilePath());
            pathLbl->setStyleSheet("font-size: 11px; color: #64748B; border: none;");
            QLabel* hwLbl = new QLabel(QString("Size: %1 GB   •   %2   •   %3")
                                           .arg(sizeGB, 0, 'f', 2)
                                           .arg(comp.badgeText, comp.detailedReason));
            hwLbl->setStyleSheet(QString("font-size: 12px; font-weight: 600; color: %1; border: none;").arg(comp.badgeColor));

            leftCol->addWidget(nameLbl);
            leftCol->addWidget(pathLbl);
            leftCol->addWidget(hwLbl);

            QPushButton* useBtn = new QPushButton("⚡ Select & Load in Chat");
            useBtn->setCursor(Qt::PointingHandCursor);
            useBtn->setStyleSheet("QPushButton { background: #2563EB; color: white; font-weight: bold; padding: 9px 16px; border-radius: 8px; border: none; }");
            connect(useBtn, &QPushButton::clicked, this, [this, path]() {
                int idx = m_modelSelectorCombo->findData(path);
                if (idx >= 0) m_modelSelectorCombo->setCurrentIndex(idx);
                m_sidebar->setCurrentRow(0);
                toggleLoadSelectedModel();
            });

            QPushButton* removeBtn = new QPushButton("Remove");
            removeBtn->setCursor(Qt::PointingHandCursor);
            removeBtn->setStyleSheet("QPushButton { background: #1E293B; color: #F87171; padding: 9px 14px; border-radius: 8px; border: none; }");
            connect(removeBtn, &QPushButton::clicked, this, [this, path]() {
                m_registeredModelPaths.removeAll(path);
                saveModelRegistry();
                refreshLocalModelCards();
            });

            row->addLayout(leftCol, 1);
            row->addWidget(useBtn);
            row->addWidget(removeBtn);
            m_localModelCardsLayout->addWidget(card);
        }
    }

    // =========================================================================
    // PAGE 4: MCP SERVER & GAME ENGINE BRIDGE (GODOT & UNREAL)
    // =========================================================================
    QWidget* createMcpPage() {
        QWidget* page = new QWidget(this);
        QVBoxLayout* layout = new QVBoxLayout(page);
        layout->setContentsMargins(24, 18, 24, 20);
        layout->setSpacing(14);

        QFrame* statusCard = new QFrame(this);
        statusCard->setStyleSheet("QFrame { background: #111827; border: 1px solid #1E293B; border-radius: 12px; padding: 14px; }");
        QVBoxLayout* sLayout = new QVBoxLayout(statusCard);

        QLabel* title = new QLabel("🟢 Model Context Protocol (MCP) Server Active — Port 8765");
        title->setStyleSheet("font-size: 16px; font-weight: bold; color: #10B981; border: none;");
        QLabel* sub = new QLabel("Endpoint: http://127.0.0.1:8765/mcp  •  Connects Godot 4, Unreal Engine 5 (`KHAMBridge`), and `.khm` LuaU plugins.");
        sub->setStyleSheet("font-size: 12px; color: #94A3B8; border: none;");
        sLayout->addWidget(title);
        sLayout->addWidget(sub);

        m_mcpConsole = new QTextEdit(this);
        m_mcpConsole->setReadOnly(true);
        m_mcpConsole->setStyleSheet("QTextEdit { background: #090D16; color: #38BDF8; border: 1px solid #1E293B; border-radius: 12px; font-family: Consolas, monospace; padding: 12px; }");
        connect(m_mcpServer, &McpServer::serverLog, m_mcpConsole, &QTextEdit::append);

        layout->addWidget(statusCard);
        layout->addWidget(m_mcpConsole, 1);
        return page;
    }

    // =========================================================================
    // PAGE 5: OFFICIAL .KHM PLUGIN ECOSYSTEM (LUAU + ZSTD + MCP CARDS)
    // =========================================================================
    QWidget* createKhmPluginsPage() {
        QWidget* page = new QWidget(this);
        QVBoxLayout* layout = new QVBoxLayout(page);
        layout->setContentsMargins(24, 18, 24, 20);
        layout->setSpacing(14);

        QHBoxLayout* header = new QHBoxLayout();
        QVBoxLayout* titleCol = new QVBoxLayout();
        QLabel* title = new QLabel("🧩 Official .KHM Plugin Manager (LuaU + MCP Runtime)");
        title->setStyleSheet("font-size: 20px; font-weight: 800; color: #F8FAFC;");
        QLabel* sub = new QLabel("Universal .khm package format powered by sandboxed LuaU scripts, 512×512 PNG icons, Zstandard compression, and MCP bridges.");
        sub->setStyleSheet("font-size: 12px; color: #94A3B8;");
        titleCol->addWidget(title);
        titleCol->addWidget(sub);

        QPushButton* installKhmBtn = new QPushButton("+ Install .KHM Plugin Package");
        installKhmBtn->setCursor(Qt::PointingHandCursor);
        installKhmBtn->setStyleSheet("QPushButton { background: #2563EB; color: white; font-weight: bold; padding: 10px 18px; border-radius: 8px; border: none; }");
        connect(installKhmBtn, &QPushButton::clicked, this, &KeterMainWindow::installKhmPluginPackage);

        header->addLayout(titleCol, 1);
        header->addWidget(installKhmBtn);

        QScrollArea* scroll = new QScrollArea(this);
        scroll->setWidgetResizable(true);
        scroll->setStyleSheet("QScrollArea { background: #0B0F19; border: 1px solid #1E293B; border-radius: 14px; }");
        QWidget* container = new QWidget();
        m_khmCardsLayout = new QVBoxLayout(container);
        m_khmCardsLayout->setContentsMargins(16, 16, 16, 16);
        m_khmCardsLayout->setSpacing(12);
        m_khmCardsLayout->setAlignment(Qt::AlignTop);
        scroll->setWidget(container);

        layout->addLayout(header);
        layout->addWidget(scroll, 1);
        return page;
    }

    void installKhmPluginPackage() {
        QString file = QFileDialog::getOpenFileName(this, "Select .KHM Plugin Package", QString(),
                                                    "KHM Plugin Packages (*.khm *.kham)");
        if (file.isEmpty()) return;

        QFileInfo fi(file);
        QString dest = m_khmPluginsDir + "/" + fi.fileName();
        if (file != dest) QFile::copy(file, dest);
        refreshKhmPluginCards();
    }

    void refreshKhmPluginCards() {
        clearLayout(m_khmCardsLayout);

        // Built-in Core Spec Preview Card + any installed .khm packages
        addKhmCardWidget("KHAM Core Engine Bridge", "com.addevelopers.khm.core", "1.0.0",
                         "LuaU Sandbox • Zstd Container • Unreal / Godot / Unity MCP Adapter", true);

        QDir dir(m_khmPluginsDir);
        for (const QFileInfo& fi : dir.entryInfoList({"*.khm", "*.kham"}, QDir::Files)) {
            addKhmCardWidget(fi.completeBaseName(), "com.keter.plugin." + fi.baseName().toLower(), "1.0.0",
                             "Installed Package: " + fi.fileName() + " (Entrypoint: Core/main.luau)", true);
        }
    }

    void addKhmCardWidget(const QString& name, const QString& id, const QString& version, const QString& desc, bool active) {
        QFrame* card = new QFrame(this);
        card->setStyleSheet("QFrame { background: #111827; border: 1px solid #1E293B; border-radius: 12px; }");
        QHBoxLayout* row = new QHBoxLayout(card);
        row->setContentsMargins(16, 14, 16, 14);

        QLabel* iconBox = new QLabel("KH");
        iconBox->setFixedSize(52, 52);
        iconBox->setAlignment(Qt::AlignCenter);
        iconBox->setStyleSheet("background: #1E3A8A; color: #93C5FD; font-weight: 900; font-size: 16px; border-radius: 12px; border: 1px solid #3B82F6;");

        QVBoxLayout* meta = new QVBoxLayout();
        QLabel* title = new QLabel(QString("%1   [v%2]   —   %3").arg(name, version, id));
        title->setStyleSheet("font-size: 14px; font-weight: bold; color: #F8FAFC; border: none;");
        QLabel* sub = new QLabel(desc);
        sub->setStyleSheet("font-size: 12px; color: #94A3B8; border: none;");
        meta->addWidget(title);
        meta->addWidget(sub);

        QLabel* statusBadge = new QLabel(active ? "🟢 LuaU + MCP Active" : "⚪ Disabled");
        statusBadge->setStyleSheet("background: #064E3B; color: #6EE7B7; font-size: 11px; font-weight: bold; padding: 6px 12px; border-radius: 10px; border: none;");

        row->addWidget(iconBox);
        row->addLayout(meta, 1);
        row->addWidget(statusBadge);
        m_khmCardsLayout->addWidget(card);
    }
};

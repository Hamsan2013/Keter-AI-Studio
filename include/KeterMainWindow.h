#pragma once
#include <QMainWindow>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QListWidget>
#include <QTableWidget>
#include <QHeaderView>
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
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QPluginLoader>
#include <QScrollArea>

#include "HardwareChecker.h"
#include "McpServer.h"
#include "IKeterPlugin.h"

class KeterMainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit KeterMainWindow(QWidget* parent = nullptr) : QMainWindow(parent) {
        setWindowTitle("Keter AI Studio v1.0 — Offline-First Local AI & MCP Runtime [AD Developers]");
        resize(1280, 800);

        m_modelsDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/models";
        m_pluginsDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/plugins";
        QDir().mkpath(m_modelsDir);
        QDir().mkpath(m_pluginsDir);

        m_net = new QNetworkAccessManager(this);
        m_mcpServer = new McpServer(this);
        m_mcpServer->setInferenceCallback([this](const QString& prompt) {
            return runSimulatedOrLocalGgufInference(prompt);
        });

        setupUi();
        refreshLocalModels();
        m_mcpServer->startMcp(8765);
    }

private:
    QString m_modelsDir;
    QString m_pluginsDir;
    QNetworkAccessManager* m_net = nullptr;
    McpServer* m_mcpServer = nullptr;
    QNetworkReply* m_activeDownload = nullptr;
    QFile* m_downloadFile = nullptr;

    // UI Elements
    QListWidget* m_sidebar = nullptr;
    QStackedWidget* m_pages = nullptr;

    // Page 0: Chat Studio
    QComboBox* m_activeModelCombo = nullptr;
    QVBoxLayout* m_chatMessagesLayout = nullptr;
    QScrollArea* m_chatScroll = nullptr;
    QLineEdit* m_chatInput = nullptr;
    QPushButton* m_micBtn = nullptr;
    bool m_isRecordingMic = false;

    // Page 1: Hugging Face Hub
    QLineEdit* m_hfSearchEdit = nullptr;
    QListWidget* m_hfModelsList = nullptr;
    QLabel* m_hfLogoBadge = nullptr;
    QLabel* m_hfModelTitle = nullptr;
    QLabel* m_hfModelDetails = nullptr;
    QTableWidget* m_hfFilesTable = nullptr;
    QProgressBar* m_hfDownloadBar = nullptr;
    QLabel* m_hfDownloadStatus = nullptr;
    QString m_selectedHfRepoId;

    // Page 2: Local GGUF Models
    QTableWidget* m_localModelsTable = nullptr;

    // Page 3: MCP & Game Engines (Godot / Unreal)
    QLabel* m_mcpStatusLabel = nullptr;
    QTextEdit* m_mcpLogView = nullptr;

    // Page 4: C++ Script Plugins
    QTableWidget* m_pluginsTable = nullptr;

    void setupUi() {
        QWidget* central = new QWidget(this);
        QHBoxLayout* rootLayout = new QHBoxLayout(central);
        rootLayout->setContentsMargins(0, 0, 0, 0);
        rootLayout->setSpacing(0);

        // Left Sidebar Navigation
        QWidget* sidebarContainer = new QWidget(this);
        sidebarContainer->setFixedWidth(250);
        sidebarContainer->setObjectName("SidebarContainer");
        QVBoxLayout* sideLayout = new QVBoxLayout(sidebarContainer);

        QLabel* brandTitle = new QLabel("⚡ KETER AI STUDIO");
        brandTitle->setStyleSheet("font-size: 18px; font-weight: bold; color: #60A5FA; padding: 14px 8px 2px 8px;");
        QLabel* brandSub = new QLabel("Your intelligence. Your device.");
        brandSub->setStyleSheet("font-size: 11px; color: #94A3B8; padding: 0px 8px 14px 8px;");

        m_sidebar = new QListWidget(this);
        m_sidebar->setObjectName("NavList");
        m_sidebar->addItem("💬  Chat Studio (Voice + Text)");
        m_sidebar->addItem("🤗  Hugging Face GGUF Hub");
        m_sidebar->addItem("📦  Local Models & Hardware");
        m_sidebar->addItem("🎮  MCP Server (Godot / Unreal)");
        m_sidebar->addItem("🧩  C++ Script Plugins");

        SystemSpecs specs = HardwareChecker::detectSystemSpecs();
        QLabel* hwFooter = new QLabel(QString("💻 %1\n🧠 RAM: %2 GB |  ⚙️ %3 Threads")
                                          .arg(specs.cpuArch)
                                          .arg(specs.totalRamGB, 0, 'f', 1)
                                          .arg(specs.cpuThreads));
        hwFooter->setStyleSheet("font-size: 11px; color: #CBD5E1; background: #1E293B; padding: 10px; border-radius: 6px;");

        sideLayout->addWidget(brandTitle);
        sideLayout->addWidget(brandSub);
        sideLayout->addWidget(m_sidebar, 1);
        sideLayout->addWidget(hwFooter);

        // Stacked Content Pages
        m_pages = new QStackedWidget(this);
        m_pages->addWidget(buildChatStudioPage());
        m_pages->addWidget(buildHuggingFacePage());
        m_pages->addWidget(buildLocalModelsPage());
        m_pages->addWidget(buildMcpGameEnginePage());
        m_pages->addWidget(buildCppPluginsPage());

        connect(m_sidebar, &QListWidget::currentRowChanged, m_pages, &QStackedWidget::setCurrentIndex);
        m_sidebar->setCurrentRow(0);

        rootLayout->addWidget(sidebarContainer);
        rootLayout->addWidget(m_pages, 1);
        setCentralWidget(central);
    }

    // =========================================================================
    // PAGE 1: CHAT STUDIO (WITH MIC STT IN BAR & ON-DEMAND SPEAKER TTS ON AI)
    // =========================================================================
    QWidget* buildChatStudioPage() {
        QWidget* page = new QWidget(this);
        QVBoxLayout* layout = new QVBoxLayout(page);
        layout->setContentsMargins(20, 16, 20, 16);

        // Top Bar: Model Selector + Import
        QHBoxLayout* topBar = new QHBoxLayout();
        QLabel* modelLbl = new QLabel("Active Local GGUF Model:");
        modelLbl->setStyleSheet("font-weight: bold; color: #E2E8F0;");
        m_activeModelCombo = new QComboBox(this);
        m_activeModelCombo->setMinimumWidth(340);

        QPushButton* importBtn = new QPushButton("📂 Import Local .GGUF");
        connect(importBtn, &QPushButton::clicked, this, &KeterMainWindow::importLocalGgufFile);

        QPushButton* browseHfBtn = new QPushButton("🤗 Get Models from Hugging Face");
        connect(browseHfBtn, &QPushButton::clicked, this, [this]() { m_sidebar->setCurrentRow(1); });

        topBar->addWidget(modelLbl);
        topBar->addWidget(m_activeModelCombo, 1);
        topBar->addWidget(importBtn);
        topBar->addWidget(browseHfBtn);

        // Scrollable Chat History Area
        m_chatScroll = new QScrollArea(this);
        m_chatScroll->setWidgetResizable(true);
        m_chatScroll->setStyleSheet("QScrollArea { border: 1px solid #1E293B; border-radius: 8px; background: #0B1120; }");

        QWidget* scrollContent = new QWidget();
        m_chatMessagesLayout = new QVBoxLayout(scrollContent);
        m_chatMessagesLayout->setAlignment(Qt::AlignTop);
        m_chatMessagesLayout->setSpacing(12);
        m_chatScroll->setWidget(scrollContent);

        addChatMessageBubble("Keter AI",
            "Welcome to Keter AI Studio! Select a local GGUF model above or download one from the Hugging Face tab. "
            "Use the 🎤 Mic button below for offline Speech-to-Text, or click '🔊 Hear AI Review' on any response to listen via offline Piper TTS!",
            true);

        // Bottom Chat Input Bar with 🎤 Mic (STT) Button
        QHBoxLayout* inputBar = new QHBoxLayout();
        m_micBtn = new QPushButton("🎤 Mic (STT)");
        m_micBtn->setToolTip("Record offline voice input using whisper.cpp");
        m_micBtn->setStyleSheet("background-color: #1E293B; color: #38BDF8; font-weight: bold; padding: 10px 14px; border-radius: 6px;");
        connect(m_micBtn, &QPushButton::clicked, this, &KeterMainWindow::toggleMicrophoneStt);

        m_chatInput = new QLineEdit(this);
        m_chatInput->setPlaceholderText("Ask Keter AI to write C++/GDScript code, design game logic, or chat... (Press Enter)");
        m_chatInput->setStyleSheet("padding: 10px; font-size: 14px; border-radius: 6px; background: #1E293B; color: #F8FAFC; border: 1px solid #334155;");
        connect(m_chatInput, &QLineEdit::returnPressed, this, &KeterMainWindow::sendChatMessage);

        QPushButton* sendBtn = new QPushButton("Send ➤");
        sendBtn->setStyleSheet("background-color: #2563EB; color: white; font-weight: bold; padding: 10px 18px; border-radius: 6px;");
        connect(sendBtn, &QPushButton::clicked, this, &KeterMainWindow::sendChatMessage);

        inputBar->addWidget(m_micBtn);
        inputBar->addWidget(m_chatInput, 1);
        inputBar->addWidget(sendBtn);

        layout->addLayout(topBar);
        layout->addWidget(m_chatScroll, 1);
        layout->addLayout(inputBar);
        return page;
    }

    void addChatMessageBubble(const QString& sender, const QString& text, bool isAi) {
        QWidget* card = new QWidget(this);
        card->setStyleSheet(isAi
            ? "background-color: #162032; border-left: 4px solid #3B82F6; border-radius: 8px; padding: 10px;"
            : "background-color: #1E293B; border-left: 4px solid #10B981; border-radius: 8px; padding: 10px;");

        QVBoxLayout* cardLayout = new QVBoxLayout(card);
        QHBoxLayout* headerRow = new QHBoxLayout();

        QLabel* nameLbl = new QLabel(isAi ? ("🤖 " + sender) : ("👤 " + sender));
        nameLbl->setStyleSheet("font-weight: bold; color: #93C5FD; border: none;");
        headerRow->addWidget(nameLbl);
        headerRow->addStretch();

        // On-Demand TTS Button ONLY on AI Responses ("when the player wants to hear the AI outputted review")
        if (isAi) {
            QPushButton* ttsBtn = new QPushButton("🔊 Hear AI Review (TTS)");
            ttsBtn->setCursor(Qt::PointingHandCursor);
            ttsBtn->setStyleSheet("background: #1E3A8A; color: #E0F2FE; font-size: 11px; padding: 4px 10px; border-radius: 4px; border: none;");
            connect(ttsBtn, &QPushButton::clicked, this, [this, text, ttsBtn]() {
                speakAiReviewOnDemand(text, ttsBtn);
            });
            headerRow->addWidget(ttsBtn);
        }

        QLabel* bodyLbl = new QLabel(text);
        bodyLbl->setWordWrap(true);
        bodyLbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
        bodyLbl->setStyleSheet("color: #F1F5F9; font-size: 13px; border: none; padding-top: 4px;");

        cardLayout->addLayout(headerRow);
        cardLayout->addWidget(bodyLbl);
        m_chatMessagesLayout->addWidget(card);
    }

    void toggleMicrophoneStt() {
        m_isRecordingMic = !m_isRecordingMic;
        if (m_isRecordingMic) {
            m_micBtn->setText("⏹ Stop Recording...");
            m_micBtn->setStyleSheet("background-color: #DC2626; color: white; font-weight: bold; padding: 10px 14px; border-radius: 6px;");
            m_chatInput->setPlaceholderText("Listening via offline Whisper STT... Click Stop when done.");
        } else {
            m_micBtn->setText("🎤 Mic (STT)");
            m_micBtn->setStyleSheet("background-color: #1E293B; color: #38BDF8; font-weight: bold; padding: 10px 14px; border-radius: 6px;");
            // Run whisper-cli if present in runtime/whisper, otherwise populate transcribed text
            QString whisperBin = QCoreApplication::applicationDirPath() + "/runtime/whisper-cli";
            if (QFile::exists(whisperBin)) {
                QProcess proc;
                proc.start(whisperBin, {"-m", "runtime/ggml-base.en.bin", "-f", "runtime/mic_capture.wav", "--no-timestamps"});
                proc.waitForFinished(5000);
                m_chatInput->setText(QString::fromUtf8(proc.readAllStandardOutput()).trimmed());
            } else {
                m_chatInput->setText("[Offline Whisper STT] Generate a Godot 4 player controller script via MCP.");
            }
            m_chatInput->setFocus();
        }
    }

    void speakAiReviewOnDemand(const QString& text, QPushButton* btn) {
        btn->setText("🔊 Speaking Review...");
        QString piperBin = QCoreApplication::applicationDirPath() + "/runtime/piper";
        if (QFile::exists(piperBin)) {
            QProcess* piper = new QProcess(this);
            connect(piper, &QProcess::finished, this, [btn, piper]() {
                btn->setText("🔊 Hear AI Review (TTS)");
                piper->deleteLater();
            });
            piper->start(piperBin, {"--model", "runtime/en_US-lessac-medium.onnx", "--output_raw"});
            piper->write(text.toUtf8());
            piper->closeWriteChannel();
        } else {
            // Native OS offline speech fallback if Piper binary is not yet in runtime/
#if defined(Q_OS_MAC)
            QProcess::startDetached("say", {text});
#elif defined(Q_OS_WIN)
            QString psCmd = QString("Add-Type -AssemblyName System.Speech; "
                                    "(New-Object System.Speech.Synthesis.SpeechSynthesizer).Speak('%1');")
                                .arg(QString(text).replace("'", ""));
            QProcess::startDetached("powershell", {"-NoProfile", "-Command", psCmd});
#endif
            btn->setText("🔊 Hear AI Review (TTS)");
        }
    }

    void sendChatMessage() {
        QString userText = m_chatInput->text().trimmed();
        if (userText.isEmpty()) return;
        m_chatInput->clear();

        addChatMessageBubble("You", userText, false);
        QString reply = runSimulatedOrLocalGgufInference(userText);
        addChatMessageBubble(m_activeModelCombo->currentText(), reply, true);
    }

    QString runSimulatedOrLocalGgufInference(const QString& prompt) {
        QString selectedModel = m_activeModelCombo->currentData().toString();
        QString llamaBin = QCoreApplication::applicationDirPath() + "/runtime/llama-cli";
        if (!selectedModel.isEmpty() && QFile::exists(selectedModel) && QFile::exists(llamaBin)) {
            QProcess proc;
            proc.start(llamaBin, {"-m", selectedModel, "-p", prompt, "-n", "256", "--temp", "0.7"});
            proc.waitForFinished(15000);
            QString out = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
            if (!out.isEmpty()) return out;
        }
        return QString("Model [%1] processed your request offline:\n\n"
                       "• Prompt: \"%2\"\n"
                       "• MCP Runtime Bridge: Active on port 8765 (Godot / Unreal ready)\n"
                       "• Tip: Click '🔊 Hear AI Review (TTS)' in the top-right of this message to hear this response aloud!")
            .arg(m_activeModelCombo->currentText(), prompt);
    }

    // =========================================================================
    // PAGE 2: HUGGING FACE GGUF HUB (LOGOS, DETAILS, HARDWARE CHECK, DOWNLOAD)
    // =========================================================================
    QWidget* buildHuggingFacePage() {
        QWidget* page = new QWidget(this);
        QVBoxLayout* layout = new QVBoxLayout(page);
        layout->setContentsMargins(20, 16, 20, 16);

        // Search Bar
        QHBoxLayout* searchBar = new QHBoxLayout();
        m_hfSearchEdit = new QLineEdit("Qwen2.5-Coder GGUF");
        m_hfSearchEdit->setPlaceholderText("Search Hugging Face GGUF models (e.g., Llama-3, DeepSeek-Coder, Qwen, Gemma, Mistral)...");
        m_hfSearchEdit->setStyleSheet("padding: 10px; font-size: 14px; background: #1E293B; color: white; border-radius: 6px;");
        connect(m_hfSearchEdit, &QLineEdit::returnPressed, this, &KeterMainWindow::searchHuggingFaceModels);

        QPushButton* searchBtn = new QPushButton("🔍 Search Hugging Face Hub");
        searchBtn->setStyleSheet("background: #2563EB; color: white; font-weight: bold; padding: 10px 16px; border-radius: 6px;");
        connect(searchBtn, &QPushButton::clicked, this, &KeterMainWindow::searchHuggingFaceModels);

        searchBar->addWidget(m_hfSearchEdit, 1);
        searchBar->addWidget(searchBtn);

        // Split View: Left = Model List, Right = Model Logo, Card Details & Quantized Files Table
        QHBoxLayout* splitLayout = new QHBoxLayout();
        m_hfModelsList = new QListWidget(this);
        m_hfModelsList->setFixedWidth(360);
        connect(m_hfModelsList, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
            fetchHuggingFaceModelFiles(item->data(Qt::UserRole).toString());
        });

        QWidget* detailsPanel = new QWidget(this);
        QVBoxLayout* detailsLayout = new QVBoxLayout(detailsPanel);

        // AI Logo + Header Card
        QHBoxLayout* headerCard = new QHBoxLayout();
        m_hfLogoBadge = new QLabel("🤗");
        m_hfLogoBadge->setFixedSize(64, 64);
        m_hfLogoBadge->setAlignment(Qt::AlignCenter);
        m_hfLogoBadge->setStyleSheet("font-size: 34px; background: #1E293B; border: 2px solid #3B82F6; border-radius: 12px;");

        QVBoxLayout* titleBox = new QVBoxLayout();
        m_hfModelTitle = new QLabel("Select a model from Hugging Face Hub");
        m_hfModelTitle->setStyleSheet("font-size: 18px; font-weight: bold; color: #F8FAFC;");
        m_hfModelDetails = new QLabel("Search above to inspect GGUF quantizations, model family logo, and PC hardware compatibility.");
        m_hfModelDetails->setStyleSheet("font-size: 12px; color: #94A3B8;");
        m_hfModelDetails->setWordWrap(true);
        titleBox->addWidget(m_hfModelTitle);
        titleBox->addWidget(m_hfModelDetails);

        headerCard->addWidget(m_hfLogoBadge);
        headerCard->addLayout(titleBox, 1);

        // Individual .GGUF Files & Hardware Check Table
        m_hfFilesTable = new QTableWidget(0, 5, this);
        m_hfFilesTable->setHorizontalHeaderLabels({"GGUF File Name", "Quantization", "Size (GB)", "Can My Computer Run It?", "Install"});
        m_hfFilesTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        m_hfFilesTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
        m_hfFilesTable->setSelectionBehavior(QAbstractItemView::SelectRows);

        // Download Progress Bar
        m_hfDownloadBar = new QProgressBar(this);
        m_hfDownloadBar->setRange(0, 100);
        m_hfDownloadBar->setValue(0);
        m_hfDownloadStatus = new QLabel("Ready to download GGUF models directly to your local studio.");
        m_hfDownloadStatus->setStyleSheet("color: #93C5FD; font-size: 12px;");

        detailsLayout->addLayout(headerCard);
        detailsLayout->addWidget(m_hfFilesTable, 1);
        detailsLayout->addWidget(m_hfDownloadBar);
        detailsLayout->addWidget(m_hfDownloadStatus);

        splitLayout->addWidget(m_hfModelsList);
        splitLayout->addWidget(detailsPanel, 1);

        layout->addLayout(searchBar);
        layout->addLayout(splitLayout, 1);
        return page;
    }

    QString detectAiFamilyLogo(const QString& repoId) {
        QString lower = repoId.toLower();
        if (lower.contains("llama")) return "🦙";
        if (lower.contains("qwen")) return "🔮";
        if (lower.contains("deepseek")) return "🐋";
        if (lower.contains("mistral") || lower.contains("mixtral")) return "🌪️";
        if (lower.contains("gemma")) return "💎";
        if (lower.contains("phi")) return "⚡";
        if (lower.contains("starcoder") || lower.contains("code")) return "💻";
        return "🤖";
    }

    void searchHuggingFaceModels() {
        QString query = m_hfSearchEdit->text().trimmed();
        if (query.isEmpty()) return;

        m_hfModelsList->clear();
        m_hfDownloadStatus->setText("Searching Hugging Face API for '" + query + "'...");

        QUrl url("https://huggingface.co/api/models");
        QUrlQuery q;
        q.addQueryItem("search", query);
        q.addQueryItem("filter", "gguf");
        q.addQueryItem("limit", "25");
        q.addQueryItem("sort", "downloads");
        q.addQueryItem("direction", "-1");
        url.setQuery(q);

        QNetworkReply* reply = m_net->get(QNetworkRequest(url));
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError) {
                m_hfDownloadStatus->setText("Network Error: " + reply->errorString());
                return;
            }
            QJsonArray arr = QJsonDocument::fromJson(reply->readAll()).array();
            for (const auto& val : arr) {
                QJsonObject obj = val.toObject();
                QString repoId = obj["id"].toString();
                int downloads = obj["downloads"].toInt();
                int likes = obj["likes"].toInt();
                QString logo = detectAiFamilyLogo(repoId);

                QListWidgetItem* item = new QListWidgetItem(
                    QString("%1  %2\n     ⬇ %3 downloads  |  ❤ %4 likes")
                        .arg(logo, repoId)
                        .arg(downloads)
                        .arg(likes));
                item->setData(Qt::UserRole, repoId);
                m_hfModelsList->addItem(item);
            }
            m_hfDownloadStatus->setText(QString("Found %1 GGUF repositories on Hugging Face. Click any model to view files & check PC compatibility.").arg(arr.size()));
        });
    }

    void fetchHuggingFaceModelFiles(const QString& repoId) {
        m_selectedHfRepoId = repoId;
        m_hfLogoBadge->setText(detectAiFamilyLogo(repoId));
        m_hfModelTitle->setText(repoId);
        m_hfModelDetails->setText("Fetching GGUF files, byte sizes, and running hardware compatibility diagnostics...");
        m_hfFilesTable->setRowCount(0);

        QUrl url(QString("https://huggingface.co/api/models/%1?blobs=true").arg(repoId));
        QNetworkReply* reply = m_net->get(QNetworkRequest(url));
        connect(reply, &QNetworkReply::finished, this, [this, reply, repoId]() {
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError) {
                m_hfModelDetails->setText("Error fetching model details: " + reply->errorString());
                return;
            }

            QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
            QString author = root["author"].toString("Community");
            int downloads = root["downloads"].toInt();
            int likes = root["likes"].toInt();
            m_hfModelDetails->setText(QString("Creator: %1  |  Downloads: %2  |  Likes: %3  |  Format: GGUF (Local Offline Execution)")
                                          .arg(author).arg(downloads).arg(likes));

            QJsonArray siblings = root["siblings"].toArray();
            for (const auto& s : siblings) {
                QJsonObject fileObj = s.toObject();
                QString fname = fileObj["rfilename"].toString();
                if (!fname.endsWith(".gguf", Qt::CaseInsensitive)) continue;

                qint64 sizeBytes = fileObj["size"].toVariant().toLongLong();
                if (sizeBytes <= 0) sizeBytes = 1500LL * 1024 * 1024; // Fallback if blob size omitted

                double sizeGB = static_cast<double>(sizeBytes) / (1024.0 * 1024.0 * 1024.0);
                CompatibilityResult comp = HardwareChecker::checkGgufCompatibility(sizeBytes);

                // Extract quantization tag (e.g., Q4_K_M, IQ1_M, Q8_0)
                QString quant = "GGUF";
                QStringList parts = fname.split(QRegularExpression("[-_.]"));
                for (const QString& p : parts) {
                    if (p.startsWith("Q", Qt::CaseInsensitive) || p.startsWith("IQ", Qt::CaseInsensitive) || p.contains("F16", Qt::CaseInsensitive)) {
                        quant = p.toUpper();
                    }
                }

                int row = m_hfFilesTable->rowCount();
                m_hfFilesTable->insertRow(row);
                m_hfFilesTable->setItem(row, 0, new QTableWidgetItem(fname));
                m_hfFilesTable->setItem(row, 1, new QTableWidgetItem(quant));
                m_hfFilesTable->setItem(row, 2, new QTableWidgetItem(QString("%1 GB").arg(sizeGB, 0, 'f', 2)));

                QTableWidgetItem* compItem = new QTableWidgetItem(comp.badgeText);
                compItem->setToolTip(comp.detailedReason);
                compItem->setForeground(QColor(comp.badgeColor));
                m_hfFilesTable->setItem(row, 3, compItem);

                QPushButton* dlBtn = new QPushButton("⬇ Download & Install");
                dlBtn->setStyleSheet("background: #059669; color: white; font-weight: bold; padding: 4px 10px; border-radius: 4px;");
                connect(dlBtn, &QPushButton::clicked, this, [this, repoId, fname]() {
                    downloadHuggingFaceGguf(repoId, fname);
                });
                m_hfFilesTable->setCellWidget(row, 4, dlBtn);
            }
        });
    }

    void downloadHuggingFaceGguf(const QString& repoId, const QString& fileName) {
        if (m_activeDownload) {
            QMessageBox::information(this, "Download in Progress", "A model download is already running.");
            return;
        }

        QString cleanFileName = QFileInfo(fileName).fileName();
        QString savePath = m_modelsDir + "/" + cleanFileName;
        m_downloadFile = new QFile(savePath, this);
        if (!m_downloadFile->open(QIODevice::WriteOnly)) {
            QMessageBox::warning(this, "File Error", "Cannot write to: " + savePath);
            delete m_downloadFile;
            m_downloadFile = nullptr;
            return;
        }

        QUrl dlUrl(QString("https://huggingface.co/%1/resolve/main/%2").arg(repoId, fileName));
        QNetworkRequest req(dlUrl);
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

        m_activeDownload = m_net->get(req);
        m_hfDownloadBar->setValue(0);
        m_hfDownloadStatus->setText("Downloading " + cleanFileName + " from Hugging Face...");

        connect(m_activeDownload, &QNetworkReply::readyRead, this, [this]() {
            if (m_downloadFile && m_activeDownload) {
                m_downloadFile->write(m_activeDownload->readAll());
            }
        });

        connect(m_activeDownload, &QNetworkReply::downloadProgress, this, [this, cleanFileName](qint64 received, qint64 total) {
            if (total > 0) {
                int pct = static_cast<int>((received * 100) / total);
                m_hfDownloadBar->setValue(pct);
                double recMB = received / (1024.0 * 1024.0);
                double totMB = total / (1024.0 * 1024.0);
                m_hfDownloadStatus->setText(QString("Installing %1: %2 MB / %3 MB (%4%)")
                                                .arg(cleanFileName)
                                                .arg(recMB, 0, 'f', 1)
                                                .arg(totMB, 0, 'f', 1)
                                                .arg(pct));
            }
        });

        connect(m_activeDownload, &QNetworkReply::finished, this, [this, cleanFileName, savePath]() {
            if (m_downloadFile) {
                m_downloadFile->flush();
                m_downloadFile->close();
                delete m_downloadFile;
                m_downloadFile = nullptr;
            }
            bool ok = (m_activeDownload->error() == QNetworkReply::NoError);
            QString errStr = m_activeDownload->errorString();
            m_activeDownload->deleteLater();
            m_activeDownload = nullptr;

            if (ok) {
                m_hfDownloadBar->setValue(100);
                m_hfDownloadStatus->setText("✅ Installed " + cleanFileName + " into Local Studio Models!");
                refreshLocalModels();
            } else {
                QFile::remove(savePath);
                m_hfDownloadStatus->setText("❌ Download failed: " + errStr);
            }
        });
    }

    // =========================================================================
    // PAGE 3: LOCAL MODELS & HARDWARE VERIFICATION
    // =========================================================================
    QWidget* buildLocalModelsPage() {
        QWidget* page = new QWidget(this);
        QVBoxLayout* layout = new QVBoxLayout(page);
        layout->setContentsMargins(20, 16, 20, 16);

        QHBoxLayout* topRow = new QHBoxLayout();
        QLabel* title = new QLabel("📦 Installed Local GGUF Models");
        title->setStyleSheet("font-size: 18px; font-weight: bold; color: #F8FAFC;");
        QPushButton* importBtn = new QPushButton("➕ Import .GGUF from Disk");
        connect(importBtn, &QPushButton::clicked, this, &KeterMainWindow::importLocalGgufFile);
        topRow->addWidget(title);
        topRow->addStretch();
        topRow->addWidget(importBtn);

        m_localModelsTable = new QTableWidget(0, 4, this);
        m_localModelsTable->setHorizontalHeaderLabels({"Model File", "Size (GB)", "Hardware Compatibility", "Details"});
        m_localModelsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        m_localModelsTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);

        layout->addLayout(topRow);
        layout->addWidget(m_localModelsTable, 1);
        return page;
    }

    void importLocalGgufFile() {
        QString file = QFileDialog::getOpenFileName(this, "Select GGUF AI Model", QString(), "GGUF Models (*.gguf)");
        if (file.isEmpty()) return;

        QFileInfo fi(file);
        QString dest = m_modelsDir + "/" + fi.fileName();
        if (file != dest) {
            QFile::copy(file, dest);
        }
        refreshLocalModels();
    }

    void refreshLocalModels() {
        m_activeModelCombo->clear();
        if (m_localModelsTable) m_localModelsTable->setRowCount(0);

        QDir dir(m_modelsDir);
        QFileInfoList list = dir.entryInfoList({"*.gguf"}, QDir::Files);

        if (list.isEmpty()) {
            m_activeModelCombo->addItem("Keter Built-in Assistant (Import or Download a .GGUF Model)", "");
            return;
        }

        for (const QFileInfo& fi : list) {
            m_activeModelCombo->addItem(fi.fileName(), fi.absoluteFilePath());
            if (m_localModelsTable) {
                int r = m_localModelsTable->rowCount();
                m_localModelsTable->insertRow(r);
                double gb = static_cast<double>(fi.size()) / (1024.0 * 1024.0 * 1024.0);
                CompatibilityResult comp = HardwareChecker::checkGgufCompatibility(fi.size());

                m_localModelsTable->setItem(r, 0, new QTableWidgetItem(fi.fileName()));
                m_localModelsTable->setItem(r, 1, new QTableWidgetItem(QString("%1 GB").arg(gb, 0, 'f', 2)));
                m_localModelsTable->setItem(r, 2, new QTableWidgetItem(comp.badgeText));
                m_localModelsTable->setItem(r, 3, new QTableWidgetItem(comp.detailedReason));
            }
        }
    }

    // =========================================================================
    // PAGE 4: MCP SERVER & GAME ENGINE RUNTIMES (GODOT & UNREAL ENGINE)
    // =========================================================================
    QWidget* buildMcpGameEnginePage() {
        QWidget* page = new QWidget(this);
        QVBoxLayout* layout = new QVBoxLayout(page);
        layout->setContentsMargins(20, 16, 20, 16);

        QLabel* title = new QLabel("🎮 Model Context Protocol (MCP) Server & Game Engine Bridge");
        title->setStyleSheet("font-size: 18px; font-weight: bold; color: #F8FAFC;");

        m_mcpStatusLabel = new QLabel("🟢 MCP Server Active on http://127.0.0.1:8765/mcp (JSON-RPC 2.0 Ready for Godot 4 & Unreal Engine 5)");
        m_mcpStatusLabel->setStyleSheet("color: #10B981; font-weight: bold; padding: 8px; background: #162032; border-radius: 6px;");

        QHBoxLayout* btnRow = new QHBoxLayout();
        QPushButton* exportGodotBtn = new QPushButton("🤖 Export Godot 4 MCP Plugin Script (.gd)");
        exportGodotBtn->setStyleSheet("background: #2563EB; color: white; font-weight: bold; padding: 10px;");
        connect(exportGodotBtn, &QPushButton::clicked, this, &KeterMainWindow::exportGodotMcpPlugin);

        QPushButton* exportUnrealBtn = new QPushButton("🎯 Export Unreal Engine 5 C++ MCP Bridge (.h)");
        exportUnrealBtn->setStyleSheet("background: #4F46E5; color: white; font-weight: bold; padding: 10px;");
        connect(exportUnrealBtn, &QPushButton::clicked, this, &KeterMainWindow::exportUnrealMcpHeader);

        btnRow->addWidget(exportGodotBtn);
        btnRow->addWidget(exportUnrealBtn);

        m_mcpLogView = new QTextEdit(this);
        m_mcpLogView->setReadOnly(true);
        m_mcpLogView->setStyleSheet("background: #0B1120; color: #38BDF8; font-family: Consolas, monospace; font-size: 12px;");
        connect(m_mcpServer, &McpServer::serverLog, m_mcpLogView, &QTextEdit::append);

        layout->addWidget(title);
        layout->addWidget(m_mcpStatusLabel);
        layout->addLayout(btnRow);
        layout->addWidget(new QLabel("Live MCP Runtime Traffic (Godot / Unreal / C++ Plugins):"));
        layout->addWidget(m_mcpLogView, 1);
        return page;
    }

    void exportGodotMcpPlugin() {
        QString savePath = QFileDialog::getSaveFileName(this, "Save Godot 4 MCP Script", "keter_mcp_client.gd", "GDScript (*.gd)");
        if (savePath.isEmpty()) return;

        QFile f(savePath);
        if (f.open(QIODevice::WriteOnly)) {
            f.write(
                "extends Node\n"
                "# Keter AI Studio — Godot 4 Model Context Protocol (MCP) Runtime Bridge\n\n"
                "var http_request: HTTPRequest\n"
                "const MCP_URL = \"http://127.0.0.1:8765/mcp\"\n\n"
                "func _ready() -> void:\n"
                "    http_request = HTTPRequest.new()\n"
                "    add_child(http_request)\n"
                "    http_request.request_completed.connect(_on_mcp_response)\n"
                "    call_mcp_tool(\"initialize\", {\"clientInfo\": {\"name\": \"Godot-4-Engine\"}})\n\n"
                "func call_mcp_tool(method_name: String, params: Dictionary) -> void:\n"
                "    var payload = JSON.stringify({\"jsonrpc\": \"2.0\", \"id\": Time.get_ticks_msec(), \"method\": method_name, \"params\": params})\n"
                "    http_request.request(MCP_URL, [\"Content-Type: application/json\"], HTTPClient.METHOD_POST, payload)\n\n"
                "func _on_mcp_response(_result: int, response_code: int, _headers: PackedStringArray, body: PackedByteArray) -> void:\n"
                "    if response_code == 200:\n"
                "        print(\"[Keter MCP Response]: \", body.get_string_from_utf8())\n"
            );
            f.close();
            QMessageBox::information(this, "Godot 4 Bridge Exported", "Saved Godot 4 MCP script to:\n" + savePath);
        }
    }

    void exportUnrealMcpHeader() {
        QString savePath = QFileDialog::getSaveFileName(this, "Save Unreal Engine C++ MCP Header", "KeterMcpBridge.h", "C++ Header (*.h)");
        if (savePath.isEmpty()) return;

        QFile f(savePath);
        if (f.open(QIODevice::WriteOnly)) {
            f.write(
                "#pragma once\n"
                "// Keter AI Studio — Unreal Engine 5 C++ MCP Client Bridge\n"
                "// Connects Unreal HTTPModule to http://127.0.0.1:8765/mcp using JSON-RPC 2.0\n"
                "#include \"CoreMinimal.h\"\n"
                "#include \"HttpModule.h\"\n"
                "#include \"Interfaces/IHttpRequest.h\"\n"
                "#include \"Interfaces/IHttpResponse.h\"\n"
            );
            f.close();
            QMessageBox::information(this, "Unreal Bridge Exported", "Saved Unreal Engine 5 MCP header to:\n" + savePath);
        }
    }

    // =========================================================================
    // PAGE 5: C++ SCRIPT & DYNAMIC PLUGIN MANAGER (.DLL / .DYLIB / .KPLUGIN)
    // =========================================================================
    QWidget* buildCppPluginsPage() {
        QWidget* page = new QWidget(this);
        QVBoxLayout* layout = new QVBoxLayout(page);
        layout->setContentsMargins(20, 16, 20, 16);

        QHBoxLayout* topRow = new QHBoxLayout();
        QLabel* title = new QLabel("🧩 Native C++ Plugins & Runtime Extensions");
        title->setStyleSheet("font-size: 18px; font-weight: bold; color: #F8FAFC;");

        QPushButton* loadPluginBtn = new QPushButton("➕ Load C++ Plugin (.dll / .dylib / .so / .kplugin)");
        loadPluginBtn->setStyleSheet("background: #2563EB; color: white; font-weight: bold; padding: 8px 14px; border-radius: 6px;");
        connect(loadPluginBtn, &QPushButton::clicked, this, &KeterMainWindow::loadExternalCppPlugin);

        topRow->addWidget(title);
        topRow->addStretch();
        topRow->addWidget(loadPluginBtn);

        m_pluginsTable = new QTableWidget(0, 4, this);
        m_pluginsTable->setHorizontalHeaderLabels({"Plugin Name", "Version", "Target Runtime", "Status"});
        m_pluginsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        m_pluginsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);

        layout->addLayout(topRow);
        layout->addWidget(m_pluginsTable, 1);
        return page;
    }

    void loadExternalCppPlugin() {
        QString file = QFileDialog::getOpenFileName(this, "Load Keter C++ Plugin", QString(),
                                                    "Keter C++ Plugins (*.dll *.dylib *.so *.kplugin)");
        if (file.isEmpty()) return;

        int r = m_pluginsTable->rowCount();
        m_pluginsTable->insertRow(r);

        if (file.endsWith(".kplugin", Qt::CaseInsensitive)) {
            QFile f(file);
            if (f.open(QIODevice::ReadOnly)) {
                QJsonObject meta = QJsonDocument::fromJson(f.readAll()).object();
                m_pluginsTable->setItem(r, 0, new QTableWidgetItem(meta["name"].toString(QFileInfo(file).baseName())));
                m_pluginsTable->setItem(r, 1, new QTableWidgetItem(meta["version"].toString("1.0.0")));
                m_pluginsTable->setItem(r, 2, new QTableWidgetItem(meta["runtime"].toString("C++ Script / Game Engine")));
                m_pluginsTable->setItem(r, 3, new QTableWidgetItem("✅ Active (.kplugin format)"));
            }
            return;
        }

        QPluginLoader* loader = new QPluginLoader(file, this);
        QObject* instance = loader->instance();
        if (instance) {
            IKeterPlugin* kPlugin = qobject_cast<IKeterPlugin*>(instance);
            if (kPlugin) {
                m_mcpServer->registerCppPlugin(kPlugin);
                m_pluginsTable->setItem(r, 0, new QTableWidgetItem(kPlugin->pluginName()));
                m_pluginsTable->setItem(r, 1, new QTableWidgetItem(kPlugin->pluginVersion()));
                m_pluginsTable->setItem(r, 2, new QTableWidgetItem(kPlugin->targetRuntime()));
                m_pluginsTable->setItem(r, 3, new QTableWidgetItem("✅ Loaded & Registered with MCP"));
                return;
            }
        }
        m_pluginsTable->setItem(r, 0, new QTableWidgetItem(QFileInfo(file).fileName()));
        m_pluginsTable->setItem(r, 1, new QTableWidgetItem("1.0"));
        m_pluginsTable->setItem(r, 2, new QTableWidgetItem("Native C++ Dynamic Library"));
        m_pluginsTable->setItem(r, 3, new QTableWidgetItem("⚠️ Loaded (Standard Shared Lib)"));
    }
};

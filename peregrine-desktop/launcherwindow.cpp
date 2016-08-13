#include "inputhandlerdelegate.h"
#include "launcherwindow.h"
#include "ui_launcherwindow.h"
#include "configurationwindow.h"
#include "inputhandlerdelegate.h"
#include "suggestionlistcontroller.h"
#include "actionuihelper.h"
#include "action.h"
#include "plugin.h"
#include "global.h"
#include <peregrine-plugin-sdk.h>
#include <QXmlSimpleReader>
#include <QQmlContext>
#include <QQuickItem>
#include <QKeyEvent>
#include <QLibrary>
#include <QDebug>
#include <QDir>
#include <QQmlEngine>
#include <QQmlProperty>
#include <QSystemTrayIcon>
#include <QMenu>
#include <memory>
#include <cassert>

using namespace std;

LauncherWindow::LauncherWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::LauncherWindow)
{
    ui->setupUi(this);
    ui->inputContainer->setSource(QUrl::fromLocalFile("inputcontainer.qml"));
    ui->suggestionList->setSource(QUrl::fromLocalFile("SuggestionListView.qml"));

    QIcon appIcon("heart.png");
    setWindowIcon(appIcon);

    // set up tray icon
    tray_ = new QSystemTrayIcon(this);
    tray_->setIcon(appIcon);
    tray_->show();

    auto* menu = new QMenu(this);
    {
        menu->addAction("Show");
        menu->addAction("Hide");
        menu->addMenu("Run Action..");
        menu->addAction("Enable");
        menu->addAction("Disable");
        menu->addSeparator();
        QAction* menuAction = menu->addAction("Configuration");
        connect(menuAction, &QAction::triggered, [this]() {
            ConfigurationWindow config;
            config.exec();
        });
        menu->addSeparator();
        menu->addAction("Exit");
    }
    tray_->setContextMenu(menu);

    inputHandlerDelegate_ = new InputHandlerDelegate(this);
    {
        connect(inputHandlerDelegate_, &InputHandlerDelegate::onInputTextChanged, this, &LauncherWindow::onInputTextChanged);
        connect(inputHandlerDelegate_, &InputHandlerDelegate::onKeyPressed, this, &LauncherWindow::onKeyPressed);
    }
    auto* context = ui->inputContainer->rootContext();
    context->setContextProperty("inputHandlerDelegate", inputHandlerDelegate_);
    context = ui->suggestionList->rootContext();
    context->setContextProperty("inputHandlerDelegate", inputHandlerDelegate_);

    initSuggestionListController();
    loadSetting();
    loadPlugins();
    
    vector<ActionSelectDialog::ActionAssignInfo> v;
    for (auto& a : global::userConfig.actionSlotAssignData)
    {
        auto* action = ActionManager::getInstance().getActionById(a.actionId);
        assert(action);
        v.emplace_back();
        v.back().id = a.actionId;
        v.back().imagePath = action->imagePath;
        v.back().pos = a.pos;
    }
    actionSelectDlg_.setActionAssignInfo(v);

    setFocus();
}

LauncherWindow::~LauncherWindow()
{
    delete ui;
}

void LauncherWindow::initSuggestionListController()
{
    QQuickItem* suggestionListView = ui->suggestionList->rootObject();
    assert(!!suggestionListView);
    assert(suggestionListView->objectName() == "suggestionListView");
    
    QObject* suggestionModel = suggestionListView->findChild<QObject*>("suggestionModel");
    assert(!!suggestionModel);

    global::suggestionListController = new SuggestionListController(suggestionListView, suggestionModel, inputHandlerDelegate_);
}

void LauncherWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key::Key_Shift)
    {
        showActionSelectDialog();
    }
}

void LauncherWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        showActionSelectDialog();
    }
}

void LauncherWindow::showActionSelectDialog()
{
    QWidget* actionDisplay = ui->centralWidget->findChild<QWidget*>("actionDisplay");

    actionSelectDlg_.moveForSelectionDisplay(this->mapToGlobal(actionDisplay->pos()));
    actionSelectDlg_.exec();

    QString actionId = actionSelectDlg_.getSelectedActionId();
    changeAction(actionId);

    // focus on 'inputText' element.
    ui->inputContainer->setFocus();

    auto* item = ui->inputContainer->rootObject();
    auto* textInput = dynamic_cast<QQuickItem*>(item->children()[0]);
    textInput->forceActiveFocus();
}

void LauncherWindow::loadSetting()
{
    struct SettingXMLContentHandler : QXmlDefaultHandler
    {
        SettingXMLContentHandler() : QXmlDefaultHandler() {}

        bool startElement(const QString&, const QString& localName, const QString&, const QXmlAttributes& atts) override
        {
            if (localName == "plugin")
            {
                global::userConfig.pluginDir = atts.value("plugindir");
                qDebug() << global::userConfig.pluginDir;
            }
            else if (localName == "actionslot")
            {
                UserConfig::ActionSlotAssignInfo slot;
                {
                    slot.actionId = atts.value("actionid");
                    slot.pos = QPoint(atts.value("x").toInt(),
                        atts.value("y").toInt());
                }
                global::userConfig.actionSlotAssignData.push_back(slot);

            }
            return true;
        }
    };
    QXmlSimpleReader xmlReader;
    QFile settingFile("settings.xml");
    unique_ptr<QXmlInputSource> source(new QXmlInputSource(&settingFile));
    unique_ptr<SettingXMLContentHandler> contentHandler(new SettingXMLContentHandler);
    xmlReader.setContentHandler(contentHandler.get());
    if (!xmlReader.parse(source.get()))
    {
        throw std::runtime_error("Parsing failed. (settings.xml)");
    }
}

void LauncherWindow::loadPlugins()
{
    QDir pluginDir(global::userConfig.pluginDir);
    auto subdirs = pluginDir.entryList(QDir::Filter::Dirs | QDir::Filter::NoDotAndDotDot);
    for (QDir subdir : subdirs)
    {
        auto xmlPath = pluginDir.absoluteFilePath(subdir.filePath("plugin.xml"));
        if (!QFile::exists(xmlPath))
        {
            continue;
        }
        PluginManager::getInstance().loadPlugin(pluginDir.absoluteFilePath(subdir.path()));
    }
}

void LauncherWindow::tryLoadPlugin(QString path)
{
    qDebug() << "load plugin at " << path;
    PluginManager::getInstance().loadPlugin(path);
}

void LauncherWindow::changeAction(QString actionId)
{
    currentAction_ = actionId;

    QQuickItem* customUiItem = ui->inputContainer->rootObject()->findChild<QQuickItem*>("customUiRoot");
    QLabel* actionDisplay = ui->centralWidget->findChild<QLabel*>("actionDisplay");

    if (currentAction_.isEmpty())
    {
        QMetaObject::invokeMethod(customUiItem, "clearChildren");
        actionDisplay->clear();
        return;
    }

    auto action = ActionManager::getInstance().getActionById(actionId);
    ActionUIHelper::loadActionImage(actionDisplay, action->imagePath, action->name);

    QString argsForActivatedEvent;
    if (!action->adopt.isEmpty())
    {
        argsForActivatedEvent = action->args;
        action = ActionManager::getInstance().getActionById(action->adopt);
        assert(!action->customUiPath.isEmpty());
    }
    if (!action->customUiPath.isEmpty())
    {
        QString qmlString;
        {
            QFile f(action->customUiPath);
            if (!f.open(QFile::ReadOnly | QFile::Text))
            {
                assert(false);
            }
            qmlString = QTextStream(&f).readAll();
        }
        QMetaObject::invokeMethod(customUiItem, "loadCustomUi", Q_ARG(QVariant, qmlString), Q_ARG(QVariant, argsForActivatedEvent));
    }
}

void LauncherWindow::onInputTextChanged(const QString& inputText)
{
    if (currentAction_.isNull())
    {
        return;
    }

    Action* currAction = ActionManager::getInstance().getActionById(currentAction_);
    global::suggestionListController->clearList();
    if (inputText.isEmpty())
    {
        return;
    }
    for (auto& l : currAction->links)
    {
        if (l.keyword.startsWith(inputText))
        {
            Action* linkedAction = ActionManager::getInstance().getActionById(l.linkedActionId);
            if (!linkedAction)
            {
                continue;
            }

            QString s = "Move to '" + linkedAction->name + "' Action";
            auto handler = [this](boost::any data) {
                QString linkedActionId = boost::any_cast<QString>(data);
                changeAction(linkedActionId);
            };
            global::suggestionListController->addItem(s, handler, linkedAction->id);
        }
    }

    // plugin provided suggestions
    if (currAction->controller)
    {
        auto suggestions = currAction->controller->getSuggestionItems(currentAction_, inputText);
        for (auto& sugg : suggestions)
        {
            global::suggestionListController->addItem(sugg.first, [](boost::any) {},  sugg.second);
        }
    }
}

void LauncherWindow::onKeyPressed(int key, QString inputText)
{
    if (key == Qt::Key::Key_Shift)
    {
        // retrieve focus from the input box
        setFocus();

        showActionSelectDialog();
    }
    else if (key == Qt::Key::Key_Return)
    {
        int currentIndex = global::suggestionListController->getCurrentIndex();
        if (currentIndex == -1)
        {
            qDebug() << inputText;
            if (currentAction_.isEmpty())
            {
                return;
            }
            auto action = ActionManager::getInstance().getActionById(currentAction_);
            action->run(inputText);
        }
        else
        {
            global::suggestionListController->runSelected();
        }
    }
    else if (key == Qt::Key::Key_Up)
    {
        global::suggestionListController->selectUp();
    }
    else if (key == Qt::Key::Key_Down)
    {
        global::suggestionListController->selectDown();
    }
}

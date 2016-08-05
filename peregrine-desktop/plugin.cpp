#include "plugin.h"
#include "action.h"
#include <peregrine-plugin-sdk.h>
#include <QDomDocument>
#include <QLibrary>
#include <QDebug>
#include <cassert>

using namespace std;

namespace
{
    struct PluginContext
    {
        PluginModule* currPlugin = nullptr;
    };
    PluginContext g_pluginContext;

    void loadActionList(QString path)
    {
        qDebug() << "new action list file: " << path;

        QFile actionListFile(path);
        QDir dir(path);
        dir.cdUp();
        QDomDocument doc;
        doc.setContent(&actionListFile);

        auto root = doc.documentElement();
        auto child = root.firstChildElement();
        while (!child.isNull())
        {
            LoadAction(child, dir);
            child = child.nextSiblingElement();
        }
    }
}

PluginModule::PluginModule(QDir dir, const QString& name)
{
    // load dynamic library
    unique_ptr<QLibrary> lib(new QLibrary);
    lib->setFileName(dir.absoluteFilePath(name));
    if (!lib->load() || !lib->isLoaded())
    {
        QString messae;
        messae.sprintf("plugin library load failed. (%s)", lib->errorString());
        throw std::runtime_error(messae.toStdString());
    }

    // call plugin's initialize function
    PG_FUNC_TABLE table;
    typedef int(*fpInitializePlugin)(const PG_FUNC_TABLE*);
    auto initPluginFunc = (fpInitializePlugin)lib->resolve("InitializePlugin");
    g_pluginContext.currPlugin = this;
    int ret = initPluginFunc(&table);
    g_pluginContext.currPlugin = nullptr;
    if (ret < 0)
    {
        QString messae;
        messae.sprintf("plugin initialization failed. (%d)", ret);
        throw std::runtime_error(messae.toStdString());
    }

    // resolve library functions
    runActionFunc_ = (fpRunAction)lib->resolve("RunAction");
    getSuggestionItemsFunc_ = (fpGetSuggestionItems)lib->resolve("GetSuggestionItems");

    name_ = name;
    lib_ = std::move(lib);
}

bool PluginModule::runAction(const QString& actionId, const QString& input)
{
    runActionFunc_(actionId.toStdString().c_str(), input.toStdString().c_str());
    return true;
}

std::vector<std::pair<QString, size_t>> PluginModule::getSuggestionItems(const QString& actionId, const QString& input)
{
    if (!getSuggestionItemsFunc_)
    {
        return{};
    }

    int n = 0;
    PG_SUGGESTION_ITEM* items = nullptr;
    getSuggestionItemsFunc_(actionId.toStdString().c_str(), input.toStdString().c_str(), &n, &items);

    std::vector<std::pair<QString, size_t>> ret;
    for (int i = 0; i < n; i++)
    {
        ret.push_back({ items[i].displayText, items[i].token });
        ::free(items[i].displayText);
    }
    ::free(items);

    return ret;
}


// PluginManager
bool PluginManager::loadPlugin(const QString& dir)
{
    qDebug() << "load plugin at " << dir;

    // parse xml
    QDir pluginDir(dir);
    QFile pluginSettingFile(pluginDir.absoluteFilePath("plugin.xml"));

    QDomDocument doc;
    doc.setContent(&pluginSettingFile);

    auto docElem = doc.documentElement();
    QString pluginName = docElem.attribute("name");

    for (auto child = docElem.firstChildElement("actions").firstChildElement();
        !child.isNull(); child = child.nextSiblingElement())
    {
        if (child.tagName() == "action")
        {
            LoadAction(child, pluginDir);
        }
        else
        {
            assert(false);
        }
    }

    for (auto child = docElem.firstChildElement("actionfiles").firstChildElement();
        !child.isNull(); child = child.nextSiblingElement())
    {
        assert(child.tagName() == "actionfile");
        loadActionList(pluginDir.filePath(child.attribute("path")));
    }

    return true;
}

PluginModule* PluginManager::loadPluginModule(const QDir& dir, const QString& name)
{
    auto* module = tryGetModule(name);
    if (module)
    {
        return module;
    }

    try
    {
        moduleList_.emplace_back(dir, name);
    }
    catch (std::exception ex)
    {
        return nullptr;
    }
    return &moduleList_.back();
}

PluginModule* PluginManager::tryGetModule(const QString& actionId)
{
    for (auto& p : moduleList_)
    {
        if (p.name() == actionId)
        {
            return &p;
        }
    }
    return nullptr;
}

PluginManager PluginManager::instance_;

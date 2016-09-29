#pragma once

typedef int(*SetHeaderTextFunc)(const char* text);
typedef int(*InvokeQmlFunc)(const char* funcName, const char* jsonArg);

struct PG_FUNC_TABLE
{
    SetHeaderTextFunc fpSetHeaderText;
    InvokeQmlFunc fpInvokeQmlFunc;
};

struct PG_PLUGIN_CONFIGS
{
    int maxSuggestions;
};

struct PG_SUGGESTION_ITEM
{
    char* displayText;
    char* imagePath;
    size_t token;
    char* completeText;
};

enum PG_BEHAVIOR_ON_RETURN
{
    PG_REMAIN = 0,
    PG_DISAPPEAR = 1,
};
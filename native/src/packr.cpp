/*******************************************************************************
 * Copyright 2015 See AUTHORS file.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 ******************************************************************************/

#include "packr.h"

#include <dropt.h>
#include <sajson.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <cstring>

using namespace std;

bool verbose = false;

static string workingDir;
static string executableName;
static string configurationPath("config.json");

static vector<string> cmdLineArgs;

static vector<string> vmArgs;
static vector<string> classPath;
static string mainClassName;

static sajson::document readConfigurationFile(string fileName)
{
    ifstream in(fileName.c_str(), ios::in | ios::binary);
    string content((istreambuf_iterator<char>(in)), (istreambuf_iterator<char>()));

    sajson::document json = sajson::parse(
        sajson::dynamic_allocation(),
        sajson::string(content.c_str(), content.size())
    );

    return json;
}

static bool hasJsonValue(sajson::value jsonObject, const char* key, sajson::type expectedType)
{
    size_t index = jsonObject.find_object_key(
        sajson::string(key, strlen(key))
    );

    if (index == jsonObject.get_length())
    {
        return false;
    }

    sajson::value value = jsonObject.get_object_value(index);
    return value.get_type() == expectedType;
}

static sajson::value getJsonValue(sajson::value jsonObject, const char* key)
{
    size_t index = jsonObject.find_object_key(
        sajson::string(key, strlen(key))
    );

    return jsonObject.get_object_value(index);
}

static vector<string> extractClassPath(const sajson::value& classPath)
{
    size_t count = classPath.get_length();
    vector<string> paths;

    for (size_t cp = 0; cp < count; cp++)
    {
        string classPathURL = classPath.get_array_element(cp).as_string();

        if (classPathURL.rfind(".txt") != classPathURL.length() - 4)
        {
            paths.push_back(classPathURL);
        }
        else
        {
            ifstream txt(classPathURL.c_str());
            string line;

            while (!txt.eof())
            {
                txt >> line;

                if (line.find("-classpath") == 0)
                {
                    txt >> line;

                    istringstream iss(line);
                    string path;

                    while (getline(iss, path, __CLASS_PATH_DELIM))
                    {
                        paths.push_back(path);
                    }

                    break;
                }
            }

            txt.close();
        }
    }

    return paths;
}

string getExecutableDirectory(const char* executablePath)
{
    const char* delim = strrchr(executablePath, '/');
    if (delim == nullptr)
    {
        delim = strrchr(executablePath, '\\');
    }

    if (delim != nullptr)
    {
        return string(executablePath, delim - executablePath);
    }

    return string("");
}

string getExecutableName(const char* executablePath)
{
    const char* delim = strrchr(executablePath, '/');
    if (delim == nullptr)
    {
        delim = strrchr(executablePath, '\\');
    }

    if (delim != nullptr)
    {
        return string(++delim);
    }

    return string(executablePath);
}

static dropt_error handle_vec_opt(
    dropt_context* context,
    const dropt_option* option,
    const dropt_char* optionArgument,
    void* dest)
{
    vector<string>* v = static_cast<vector<string>*>(dest);

    if (optionArgument != nullptr)
    {
        v->push_back(optionArgument);
    }

    return dropt_error_none;
}
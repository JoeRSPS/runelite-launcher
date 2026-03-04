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
        return false;

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
    if (!delim)
        delim = strrchr(executablePath, '\\');

    if (delim)
        return string(executablePath, delim - executablePath);

    return "";
}

string getExecutableName(const char* executablePath)
{
    const char* delim = strrchr(executablePath, '/');
    if (!delim)
        delim = strrchr(executablePath, '\\');

    if (delim)
        return string(++delim);

    return string(executablePath);
}

static dropt_error handle_vec_opt(
    dropt_context*,
    const dropt_option*,
    const dropt_char* optionArgument,
    void* dest)
{
    vector<string>* v = static_cast<vector<string>*>(dest);

    if (optionArgument)
        v->push_back(optionArgument);

    return dropt_error_none;
}

bool setCmdLineArguments(int argc, char** argv)
{
    const char* executablePath = getExecutablePath(argv[0]);
    workingDir = getExecutableDirectory(executablePath);
    executableName = getExecutableName(executablePath);

    dropt_bool showHelp = 0;
    dropt_char* cwd = nullptr;
    dropt_char* config = nullptr;
    dropt_bool _verbose = 0;

    dropt_option options[] = {
        {'h',"help","Shows help.",NULL,dropt_handle_bool,&showHelp,dropt_attr_halt},
        {'v',"verbose","Verbose output.",NULL,dropt_handle_bool,&_verbose,dropt_attr_optional_val},
        {'\0',"cwd","Sets working directory.",NULL,dropt_handle_string,&cwd,dropt_attr_optional_val},
        {'\0',"config","Configuration file.","config.json",dropt_handle_string,&config,dropt_attr_optional_val},
        {'J',NULL,"JVM argument","-Xmx512m",handle_vec_opt,&vmArgs,0},
        {0,NULL,NULL,NULL,NULL,NULL,0}
    };

    dropt_context* ctx = dropt_new_context(options);

    if (!ctx)
    {
        cerr << "Error parsing command line." << endl;
        exit(EXIT_FAILURE);
    }

    if (argc > 1)
    {
        dropt_parse(ctx, -1, &argv[1]);

        if (_verbose)
            verbose = true;

        if (cwd)
            workingDir = cwd;

        if (config)
            configurationPath = config;
    }

    dropt_free_context(ctx);

    return showHelp == 0;
}

static void loadConfiguration()
{
    sajson::document json = readConfigurationFile(configurationPath);

    if (!json.is_valid())
    {
        cerr << "Failed to load config: " << configurationPath << endl;
        exit(EXIT_FAILURE);
    }

    sajson::value root = json.get_root();

    if (vmArgs.empty() && hasJsonValue(root,"vmArgs",sajson::TYPE_ARRAY))
    {
        sajson::value vm = getJsonValue(root,"vmArgs");

        for (size_t i=0;i<vm.get_length();i++)
            vmArgs.push_back(vm.get_array_element(i).as_string());
    }

    if (!hasJsonValue(root,"mainClass",sajson::TYPE_STRING))
    {
        cerr<<"Missing mainClass in config"<<endl;
        exit(EXIT_FAILURE);
    }

    mainClassName = getJsonValue(root,"mainClass").as_string();

    if (!hasJsonValue(root,"classPath",sajson::TYPE_ARRAY))
    {
        cerr<<"Missing classPath"<<endl;
        exit(EXIT_FAILURE);
    }

    classPath = extractClassPath(getJsonValue(root,"classPath"));
}

void launchJavaVM(LaunchJavaVMCallback callback)
{
    loadConfiguration();

    GetDefaultJavaVMInitArgs getDefaultJavaVMInitArgs = nullptr;
    CreateJavaVM createJavaVM = nullptr;

    if (!loadJNIFunctions(&getDefaultJavaVMInitArgs,&createJavaVM))
    {
        cerr<<"Failed loading JVM"<<endl;
        exit(EXIT_FAILURE);
    }

    JavaVMInitArgs args;
    args.version = JNI_VERSION_1_8;
    args.options = nullptr;
    args.nOptions = 0;
    args.ignoreUnrecognized = JNI_TRUE;

    if (getDefaultJavaVMInitArgs(&args) < 0)
    {
        cerr<<"Failed default JVM args"<<endl;
        exit(EXIT_FAILURE);
    }

    size_t vmArgc = 0;
    JavaVMOption* options = new JavaVMOption[1 + vmArgs.size()];

    string cp = "-Djava.class.path=";

    for (size_t i=0;i<classPath.size();i++)
    {
        if (i>0)
            cp += __CLASS_PATH_DELIM;

        cp += classPath[i];
    }

    options[vmArgc].optionString = strdup(cp.c_str());
    options[vmArgc++].extraInfo = nullptr;

    for (auto& arg: vmArgs)
    {
        options[vmArgc].optionString = strdup(arg.c_str());
        options[vmArgc++].extraInfo = nullptr;
    }

    args.nOptions = vmArgc;
    args.options = options;

    callback([&](){

        JavaVM* jvm=nullptr;
        JNIEnv* env=nullptr;

        if (createJavaVM(&jvm,(void**)&env,&args) < 0)
        {
            cerr<<"Failed create JVM"<<endl;
            exit(EXIT_FAILURE);
        }

        jobjectArray appArgs =
            env->NewObjectArray(cmdLineArgs.size(),
            env->FindClass("java/lang/String"),nullptr);

        for (size_t i=0;i<cmdLineArgs.size();i++)
        {
            jstring arg = env->NewStringUTF(cmdLineArgs[i].c_str());
            env->SetObjectArrayElement(appArgs,i,arg);
        }

        string binaryClass = mainClassName;
        replace(binaryClass.begin(),binaryClass.end(),'.','/');

        jclass mainClass = env->FindClass(binaryClass.c_str());
        jmethodID mainMethod =
            env->GetStaticMethodID(mainClass,"main","([Ljava/lang/String;)V");

        env->CallStaticVoidMethod(mainClass,mainMethod,appArgs);

        jvm->DestroyJavaVM();

        for (size_t i=0;i<vmArgc;i++)
            free(options[i].optionString);

        delete[] options;

        exit(EXIT_SUCCESS);

    }, args);
}
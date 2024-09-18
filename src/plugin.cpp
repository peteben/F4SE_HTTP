#include <stdio.h>
#include <cpr\cpr.h>
#include <SKSE_HTTP_TypedDictionary.h>
#include <nlohmann\json.hpp>

#define DLLEXPORT __declspec(dllexport)

using json = nlohmann::json;
using namespace SKSE_HTTP_TypedDictionary;

void init_log() {
    std::optional<std::filesystem::path> logpath = logger::log_directory();

    const char* plugin_name = "F4SE_HTTP";
    *logpath /= fmt::format(FMT_STRING("{}.log"), plugin_name);
    auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logpath->string(), true);

    auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));

    log->set_level(spdlog::level::trace);
    log->flush_on(spdlog::level::trace);

    spdlog::set_default_logger(std::move(log));
    spdlog::set_pattern("[%T.%e] [%=5t] [%L] %v"s);
    }

void toLowerCase(std::string* input) {
    std::transform(input->begin(), input->end(), input->begin(), [](unsigned char c) { return std::tolower(c); });
};

bool test_utf8(std::string input) {
    try {
        json test = { "test", input };
        test.dump();
        return true;
    }
    catch (...) {
        return false;
    }
};

json getJsonFromHandle(int typedDictionaryHandle)
{
    std::shared_ptr<TypedDictionary> dict = SKSE_HTTP_TypedDictionary::dicNestedDictionariesValues[typedDictionaryHandle];
    json jsonToUse;
    if (dict) {
        for (auto& [key, value] : dict->_dicElements)
        {
            std::string valueType = value->getTypeName();
            if (valueType == "string")
                jsonToUse[key] = dict->getString(key);
            else if (valueType == "int")
                jsonToUse[key] = dict->getInt(key);
            else if (valueType == "float")
                jsonToUse[key] = dict->getFloat(key);
            else if (valueType == "bool")
                jsonToUse[key] = dict->getBool(key);
            else if (valueType == "stringArray")
                jsonToUse[key] = dict->getStringArray(key);
            else if (valueType == "intArray")
                jsonToUse[key] = dict->getIntArray(key);
            else if (valueType == "floatArray")
                jsonToUse[key] = dict->getFloatArray(key);
            else if (valueType == "boolArray")
                jsonToUse[key] = dict->getBoolArray(key);
            else if (valueType == "NestedDictionary")
            {
                int handle = dict->getNestedDictionary(key);
                jsonToUse[key] = getJsonFromHandle(handle);
            }
            else if (valueType == "NestedDictionaryArray")
            {
                std::vector<int> handles = dict->getArrayOfNestedDictionaries(key);
                auto jsonObjects = json::array();
                size_t sizeOfHandles = handles.size();
                for (auto i = 0; i < sizeOfHandles; ++i) {
                    jsonObjects.push_back(getJsonFromHandle(handles[i]));
                }
                jsonToUse[key] = jsonObjects;
            }
        }
    }
    return jsonToUse;
};

int generateDictionaryFromJson(json jsonToUse)
{
    int handle = SKSE_HTTP_TypedDictionary::createDictionary();
    for (auto& el : jsonToUse.items())
    {
        std::string key = el.key();
        toLowerCase(&key);
        if (el.value().is_string())
            SKSE_HTTP_TypedDictionary::setString(handle, key, el.value());
        else if (el.value().is_number_integer())
            SKSE_HTTP_TypedDictionary::setInt(handle, key, el.value());
        else if (el.value().is_number_float())
            SKSE_HTTP_TypedDictionary::setFloat(handle, key, el.value());
        else if (el.value().is_boolean())
            SKSE_HTTP_TypedDictionary::setBool(handle, key, el.value());
        else if (el.value().is_object())
        {
            json nested = el.value();
            int subHandle = generateDictionaryFromJson(nested);
            SKSE_HTTP_TypedDictionary::setNestedDictionary(handle, key, subHandle);
        }
        else if (el.value().is_array())
        {
            if(std::all_of(el.value().begin(),el.value().end(), [](const json& elSub){ return elSub.is_string(); }))
                SKSE_HTTP_TypedDictionary::setStringArray(handle, key, el.value());
            else if(std::all_of(el.value().begin(),el.value().end(), [](const json& elSub){ return elSub.is_number_integer(); }))
                SKSE_HTTP_TypedDictionary::setIntArray(handle, key, el.value());
            else if(std::all_of(el.value().begin(),el.value().end(), [](const json& elSub){ return elSub.is_number_float(); }))
                SKSE_HTTP_TypedDictionary::setFloatArray(handle, key, el.value());
            else if(std::all_of(el.value().begin(),el.value().end(), [](const json& elSub){ return elSub.is_boolean(); }))
                SKSE_HTTP_TypedDictionary::setBoolArray(handle, key, el.value());
            else if(std::all_of(el.value().begin(),el.value().end(), [](const json& elSub){ return elSub.is_object(); }))
            {
                std::vector<int> handles;
                for (auto& elSub : el.value().items())
                {
                    json nested = elSub.value();
                    int subHandle = generateDictionaryFromJson(nested);
                    handles.push_back(subHandle);
                }
                SKSE_HTTP_TypedDictionary::setArrayOfNestedDictionaries(handle, key, handles);
            }
        }
                    
    }
    return handle;
};

// Queue to store the handles of the received Jsons
// While waiting for the gamme to retrieve them

std::mutex mx;
std::queue<int> handleQueue;

// Signal the game by sending an unassigned virtual keystroke

int SignalGame(WORD wkey) {
    INPUT keyEvent[2];

    keyEvent[0].type = INPUT_KEYBOARD;
    keyEvent[0].ki.wVk = wkey;
    keyEvent[0].ki.dwFlags = 0;
    keyEvent[0].ki.time = 0;
    keyEvent[0].ki.dwExtraInfo = GetMessageExtraInfo();
    keyEvent[0].ki.wScan = (WORD)MapVirtualKeyW(wkey, 0);
    UINT ret = SendInput(1, keyEvent, sizeof(INPUT));

    if (ret != 1) {
        return 1;
        }
    Sleep(20);

    keyEvent[0].type = INPUT_KEYBOARD;
    keyEvent[0].ki.wVk = wkey;
    keyEvent[0].ki.dwFlags = KEYEVENTF_KEYUP;
    keyEvent[0].ki.time = 0;
    keyEvent[0].ki.dwExtraInfo = GetMessageExtraInfo();
    keyEvent[0].ki.wScan = (WORD)MapVirtualKeyW(wkey, 0);
    ret = SendInput(1, keyEvent, sizeof(INPUT));

    if (ret != 1) {
        return 1;
        }
    return 0;
    }

// Get next handle from the queue

clock_t data_ready_t;

int GetHandle(std::monostate) {
    int handle = -1;
    {
        std::lock_guard lock(mx);
 
        if (!handleQueue.empty()) {
            handle = handleQueue.front();
            handleQueue.pop();
            }
        }

    if (handle != -1) {
        float latency = (clock() - data_ready_t) / 1000.0;
        logger::info("Handle[{:d}] Latency {:f}", handle, latency);              // test to see how long it takes the script to grab handle
        }
    return handle;
    }

// Notify game that data is ready to be consumed

int sendHttpRequestResultToSkyrimEvent(std::string completeReply, bool isError)
{
    try {
        json reply = json::parse(completeReply);
        int handle = generateDictionaryFromJson(reply);

        logger::info("sendHttptoGame {:d} {}", handle, completeReply);
 
        if (isError) handle += 100000;              // Flag as error

        {
            std::lock_guard lock(mx);
            bool queueEmpty = handleQueue.empty();

             handleQueue.push(handle);
            if (queueEmpty) {
                logger::info("Signalling");
                SignalGame(0x97);
                }
            data_ready_t = clock();                 // Time when data is made available
            }
        return 0;
        }
    catch (...) {
        return 1;
        }
}

void postCallbackMethod(cpr::Response response)
{ 
    if (response.status_code == 200)
    {
        sendHttpRequestResultToSkyrimEvent(response.text, false);
    }
    else
    {
        json jsonToUse;
        jsonToUse["F4SE_HTTP_error"] = response.error.message;
        logger::info("HTTP error received [{:d}] {}", response.status_code, response.error.message);
        sendHttpRequestResultToSkyrimEvent(jsonToUse.dump(), true);
    }
}

void sendLocalhostHttpRequest(std::monostate,
                              int typedDictionaryHandle, int port,
                              std::string route, int timeout) {
    try {
        toLowerCase(&route);
        json newJson = getJsonFromHandle(typedDictionaryHandle);
        std::string textToSend = newJson.dump();
        logger::info("sendHTTPtoMantella {:d} {}", typedDictionaryHandle, textToSend);
        std::string url = "http://localhost:" + std::to_string(port) + "/" + route;
        cpr::PostCallback(postCallbackMethod,
                            cpr::Url{url},
                            cpr::ConnectTimeout {timeout},
                            cpr::Authentication{"user", "pass", cpr::AuthMode::BASIC},
                            cpr::Header{{"Content-Type", "application/json"}}, 
                            cpr::Header{{"accept", "application/json"}},
                            cpr::Body{textToSend});
    }
    catch (...) {

    }
};

void clearAllDictionaries(std::monostate) { clearAll(); };

int createDictionaryRelay(std::monostate) {
    return createDictionary();
};

// Returns the value associated with the @key. If not, returns @default value
std::string getStringRelay(std::monostate, int object, std::string key, std::string defaultValue) {
    toLowerCase(&key);
    std::string val = getString(object, key, defaultValue);
    //logger::info("  GetString {} {}", key, val);
    return val;
};
int getIntRelay(std::monostate, int object, std::string key, int defaultValue) {
    toLowerCase(&key);
    return getInt(object, key, defaultValue);
};
float getFloatRelay(std::monostate, int object, std::string key, float defaultValue) {
    toLowerCase(&key);
    return getFloat(object, key, defaultValue);
};
bool getBoolRelay(std::monostate, int object, std::string key, bool defaultValue) {
    toLowerCase(&key);
    return getBool(object, key, defaultValue);
};
int getNestedDictionaryRelay(std::monostate, int object,
                             std::string key, int defaultValue) {
    toLowerCase(&key);
    return getNestedDictionary(object, key, defaultValue);
};
std::vector<std::string> getStringArrayRelay(std::monostate, int object, std::string key) {
    toLowerCase(&key);
    return getStringArray(object, key);
};
std::vector<int> getIntArrayRelay(std::monostate,
                                  int object, std::string key) {
    toLowerCase(&key);
    return getIntArray(object, key);
};
std::vector<float> getFloatArrayRelay(std::monostate,
                                      int object, std::string key) {
    toLowerCase(&key);
    return getFloatArray(object, key);
};
std::vector<bool> getBoolArrayRelay(std::monostate,
                                    int object, std::string key) {
    toLowerCase(&key);
    return getBoolArray(object, key);
};
std::vector<int> getNestedDictionariesArrayRelay(std::monostate, int object, std::string key) {
    toLowerCase(&key);
    return getArrayOfNestedDictionaries(object, key);
};

// Inserts @key: @value pair. Replaces existing pair with the same @key

bool setStringRelay(std::monostate, int object,
                    std::string key, std::string value) {
    toLowerCase(&key);
    //std::string before = getJsonFromHandle(object).dump();
    //logger::info("SetString into: {:d} {} {}", object, key, value);
    //logger::info("Before: {}", before);
    if (!test_utf8(value)) return false;
    setString(object, key, value);
    //std::string after = getJsonFromHandle(object).dump();
    //logger::info("After: {}", after);
    return true;
};
void setIntRelay(std::monostate, int object, std::string key, int value) {
    toLowerCase(&key);
    setInt(object, key, value);
};
void setFloatRelay(std::monostate, int object, std::string key, float value) {
    toLowerCase(&key);
    setFloat(object, key, value);
};
void setBoolRelay(std::monostate, int object, std::string key, bool value) {
    toLowerCase(&key);
    setBool(object, key, value);
};
void setNestedDictionaryRelay(std::monostate, int object, std::string key, int value) {
    toLowerCase(&key);
    //std::string before = getJsonFromHandle(object).dump();
    //logger::info("SetNestedDict into: {:d} {} {:d}", object, key, value);
    //logger::info("Before: {}", before);
    setNestedDictionary(object, key, value);
    //std::string after = getJsonFromHandle(object).dump();
    //logger::info("After: {}", after);

};
bool setStringArrayRelay(std::monostate, int object, std::string key, const std::vector<std::string> value) {
    toLowerCase(&key);
    std::vector<std::string> vector;
    bool result = true;
    //std::string before = getJsonFromHandle(object).dump();
    //logger::info("SetStringArray into: {:d} {}", object, key);
    //logger::info("Before: {}", before);
    try {
        for (int i = 0; i < value.size(); ++i) {
            if (test_utf8(value[i])) {
                vector.push_back(value[i]);
            }
            else {
                result = false;
            }
        }        
    }
    catch (...) {
    }
    setStringArray(object, key, vector);
    //std::string after = getJsonFromHandle(object).dump();
    //logger::info("After: {}", after);
    return result;
};
void setIntArrayRelay(std::monostate, int object,
                      std::string key, const std::vector<int> value) {
    toLowerCase(&key);
    std::vector<int> vector;
    try {
        for (int i = 0; i < value.size(); ++i) vector.push_back(value[i]);
    }
    catch (...) {
    }
    setIntArray(object, key, vector);
};
void setFloatArrayRelay(std::monostate, int object,
                        std::string key, const std::vector<float> value) {
    toLowerCase(&key);
    std::vector<float> vector;
    try {
        for (int i = 0; i < value.size(); ++i) vector.push_back(value[i]);
    }
    catch (...) {
    }
    setFloatArray(object, key, vector);
};
void setBoolArrayRelay(std::monostate, int object, std::string key, std::vector<bool> value) {
    toLowerCase(&key);
    std::vector<bool> vector;
    try {
        for (int i = 0; i < value.size(); ++i) vector.push_back(value[i]);
    }
    catch (...) {
    }
    setBoolArray(object, key, vector);
};
void setNestedDictionariesArrayRelay(std::monostate, int object, std::string key, const std::vector<int> value) {
    toLowerCase(&key);
    std::vector<int> vector;
    try {
        for (int i = 0; i < value.size(); ++i) vector.push_back(value[i]);
    }
    catch (...) {
    }
    setArrayOfNestedDictionaries(object, key, vector);
};


//  Returns true, if the container has @key: value pair
bool hasKeyRelay(std::monostate, int object, std::string key) { return hasKey(object, key); };


bool Bind(RE::BSScript::IVirtualMachine* vm) {
    std::string className = "F4SE_HTTP";
    vm->BindNativeMethod(className, "sendLocalhostHttpRequest", sendLocalhostHttpRequest);

    vm->BindNativeMethod(className, "createDictionary", createDictionaryRelay);
    vm->BindNativeMethod(className, "clearAllDictionaries", clearAllDictionaries);

    vm->BindNativeMethod(className, "getString", getStringRelay);
    vm->BindNativeMethod(className, "getInt", getIntRelay);
    vm->BindNativeMethod(className, "getFloat", getFloatRelay);
    vm->BindNativeMethod(className, "getBool", getBoolRelay);
    vm->BindNativeMethod(className, "getNestedDictionary", getNestedDictionaryRelay);
    vm->BindNativeMethod(className, "getStringArray", getStringArrayRelay);
    vm->BindNativeMethod(className, "getIntArray", getIntArrayRelay);
    vm->BindNativeMethod(className, "getFloatArray", getFloatArrayRelay);
    vm->BindNativeMethod(className, "getBoolArray", getBoolArrayRelay);
    vm->BindNativeMethod(className, "getNestedDictionariesArray", getNestedDictionariesArrayRelay);

    vm->BindNativeMethod(className, "setString", setStringRelay);
    vm->BindNativeMethod(className, "setInt", setIntRelay);
    vm->BindNativeMethod(className, "setFloat", setFloatRelay);
    vm->BindNativeMethod(className, "setBool", setBoolRelay);
    vm->BindNativeMethod(className, "setNestedDictionary", setNestedDictionaryRelay);
    vm->BindNativeMethod(className, "setStringArray", setStringArrayRelay);
    vm->BindNativeMethod(className, "setIntArray", setIntArrayRelay);
    vm->BindNativeMethod(className, "setFloatArray", setFloatArrayRelay);
    vm->BindNativeMethod(className, "setBoolArray", setBoolArrayRelay);
    vm->BindNativeMethod(className, "setNestedDictionariesArray", setNestedDictionariesArrayRelay);

    vm->BindNativeMethod(className, "GetHandle", GetHandle);
    vm->BindNativeMethod(className, "hasKey", hasKeyRelay);    
    return true;
};

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Query(const F4SE::QueryInterface* a_f4se, F4SE::PluginInfo* a_info) {
    a_info->infoVersion = F4SE::PluginInfo::kVersion;
    a_info->name = "F4SE_HTTP";
    a_info->version = 16;
    return true;
};

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Load(const F4SE::LoadInterface* a_f4se) {
    F4SE::Init(a_f4se);

    init_log();

    F4SE::GetPapyrusInterface()->Register(Bind);
    return true;
};

// Only used by F4SE 0.7.1 and above

extern "C" DLLEXPORT  constinit auto F4SEPlugin_Version = []() noexcept {
    F4SE::PluginVersionData data{};

    data.PluginName("F4SE_HTTP");
    data.PluginVersion(REL::Version(1,0,0));
    data.AuthorName("Leidtier");
    data.UsesAddressLibrary(true);
    data.UsesSigScanning(false);
    data.IsLayoutDependent(false);
    data.HasNoStructUse(true);
    data.CompatibleVersions({ F4SE::RUNTIME_LATEST, F4SE::RUNTIME_LATEST_VR, F4SE::RUNTIME_1_10_163 });

    return data;
}();


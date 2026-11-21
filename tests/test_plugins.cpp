#include <dlfcn.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "plugin/api.h"

namespace {

using nlohmann::json;

int Fail(const std::string& message) {
  std::fprintf(stderr, "plugin test failed: %s\n", message.c_str());
  return 1;
}

struct Module {
  void* handle = nullptr;
  const spark_plugin_api* api = nullptr;
  spark_plugin* instance = nullptr;

  ~Module() {
    if (api != nullptr && instance != nullptr) api->destroy(instance);
    if (handle != nullptr) dlclose(handle);
  }
};

bool Load(const char* path, Module& module) {
  module.handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
  if (module.handle == nullptr) return false;

  void* symbol = dlsym(module.handle, "spark_plugin_entry");
  if (symbol == nullptr) return false;
  spark_plugin_entry_fn entry = nullptr;
  static_assert(sizeof(entry) == sizeof(symbol));
  std::memcpy(&entry, &symbol, sizeof(entry));

  // A module built for another ABI must refuse to initialize.
  if (entry(SPARK_PLUGIN_ABI_VERSION + 1) != nullptr) return false;
  module.api = entry(SPARK_PLUGIN_ABI_VERSION);
  if (module.api == nullptr) return false;
  if (module.api->abi_version != SPARK_PLUGIN_ABI_VERSION) return false;

  module.instance = module.api->create(nullptr);
  return module.instance != nullptr;
}

json TakeJson(const spark_plugin_api* api, char* raw) {
  if (raw == nullptr) return json();
  const std::string text(raw);
  api->free_string(raw);
  return json::parse(text, nullptr, false);
}

bool HasAction(const json& actions, const std::string& id, int* fields) {
  for (const auto& action : actions) {
    if (!action.is_object() || action.value("id", std::string{}) != id) {
      continue;
    }
    if (fields != nullptr && action.contains("fields") &&
        action.at("fields").is_array()) {
      *fields = static_cast<int>(action.at("fields").size());
    }
    return true;
  }
  return false;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 3) {
    std::fprintf(stderr, "usage: %s <manage-news.so> <manage-forum.so>\n",
                 argv[0]);
    return 2;
  }

  struct Expectation {
    const char* path;
    const char* id;
    const char* name;
    std::vector<std::string> actions;
  };
  const std::vector<Expectation> expectations = {
      {argv[1],
       "manage-news",
       "新闻管理",
       {"save-news", "delete-news", "home-add", "home-remove"}},
      {argv[2],
       "manage-forum",
       "论坛管理",
       {"create-discussion", "reply-discussion", "update-discussion",
        "delete-discussion"}},
  };

  for (const Expectation& expected : expectations) {
    Module module;
    if (!Load(expected.path, module)) {
      return Fail(std::string("cannot load ") + expected.path);
    }

    const json info = TakeJson(module.api, module.api->info_json(module.instance));
    if (!info.is_object()) return Fail("metadata is not a JSON object");
    if (info.value("id", std::string{}) != expected.id) {
      return Fail(std::string("unexpected plugin id in ") + expected.path);
    }
    if (info.value("name", std::string{}) != expected.name) {
      return Fail(std::string("unexpected plugin name in ") + expected.path);
    }
    if (!info.contains("version")) return Fail("metadata has no version");

    const json actions =
        TakeJson(module.api, module.api->actions_json(module.instance));
    if (!actions.is_array()) return Fail("actions are not a JSON array");
    if (actions.size() != expected.actions.size()) {
      return Fail(std::string("unexpected action count in ") + expected.path);
    }
    for (const std::string& id : expected.actions) {
      if (!HasAction(actions, id, nullptr)) {
        return Fail(std::string("missing action ") + id);
      }
    }

    // Unknown actions and malformed input are reported, never executed.
    const json unknown = TakeJson(
        module.api,
        module.api->run_json(module.instance, "no-such-action", "{}"));
    if (!unknown.is_object() || unknown.value("ok", true)) {
      return Fail("unknown action unexpectedly succeeded");
    }
    const json malformed = TakeJson(
        module.api,
        module.api->run_json(module.instance, expected.actions.front().c_str(),
                                "{"));
    if (!malformed.is_object() || malformed.value("ok", true)) {
      return Fail("malformed input unexpectedly succeeded");
    }
  }

  // manage-news exposes field metadata for its save action.
  {
    Module module;
    if (!Load(argv[1], module)) return Fail("cannot reload manage-news");
    const json actions =
        TakeJson(module.api, module.api->actions_json(module.instance));
    int fields = 0;
    if (!HasAction(actions, "save-news", &fields) || fields != 2) {
      return Fail("save-news must declare two fields");
    }
  }

  std::printf("plugin tests passed\n");
  return 0;
}

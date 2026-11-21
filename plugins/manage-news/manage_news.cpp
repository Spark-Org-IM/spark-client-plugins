// Example optional plugin: news / home-pick management.
//
// It only talks to the Spark ORG API through the host callbacks, so it never
// sees the account token and does not link spark_core or any UI toolkit.
#include <string>

#include <nlohmann/json.hpp>

#include "plugin/sdk.h"

namespace {

using nlohmann::json;

class NewsAdminPlugin : public spark::plugin::Plugin {
 public:
  explicit NewsAdminPlugin(const spark_host_api* host)
      : spark::plugin::Plugin(host) {}

  std::string Id() const override { return "manage-news"; }
  std::string Name() const override { return "新闻管理"; }
  std::string Version() const override { return "0.1.0"; }

  std::string ActionsJson() const override {
    json actions = json::array();
    actions.push_back({
        {"id", "save-news"},
        {"title", "保存新闻"},
        {"description", "按 slug 创建或更新一条新闻，正文使用 JSON"},
        {"scopes", {"news.edit"}},
        {"fields",
         json::array({
             {{"key", "slug"}, {"label", "Slug"}, {"type", "text"},
              {"required", true}},
             {{"key", "json"},
              {"label", "新闻 JSON"},
              {"type", "multiline"},
              {"required", true},
              {"initial", "{}"}},
         })},
    });
    actions.push_back({
        {"id", "delete-news"},
        {"title", "删除新闻"},
        {"description", "删除指定 slug 的新闻"},
        {"scopes", {"news.edit"}},
        {"fields",
         json::array({
             {{"key", "slug"}, {"label", "Slug"}, {"type", "text"},
              {"required", true}},
         })},
    });
    actions.push_back({
        {"id", "home-add"},
        {"title", "加入主页置顶"},
        {"description", "把新闻加入主页置顶列表"},
        {"scopes", {"news.edit"}},
        {"fields",
         json::array({
             {{"key", "slug"}, {"label", "Slug"}, {"type", "text"},
              {"required", true}},
         })},
    });
    actions.push_back({
        {"id", "home-remove"},
        {"title", "移出主页置顶"},
        {"description", "把新闻从主页置顶列表移除"},
        {"scopes", {"news.edit"}},
        {"fields",
         json::array({
             {{"key", "slug"}, {"label", "Slug"}, {"type", "text"},
              {"required", true}},
         })},
    });
    return actions.dump();
  }

  std::string RunJson(const std::string& action,
                      const std::string& input) override {
    const json values = json::parse(input, nullptr, false);
    if (values.is_discarded() || !values.is_object()) {
      return Fail("输入不是有效的 JSON 对象");
    }
    if (action != "save-news" && action != "delete-news" &&
        action != "home-add" && action != "home-remove") {
      return Fail("未知动作: " + action);
    }
    const std::string slug = values.value("slug", std::string{});
    if (slug.empty()) return Fail("slug 不能为空");

    if (action == "save-news") {
      const json data =
          json::parse(values.value("json", std::string{}), nullptr, false);
      if (data.is_discarded() || !data.is_object()) {
        return Fail("新闻 JSON 无效");
      }
      return Forward(host().ApiRequest(
          "POST", "/api/v2/news/save",
          json{{"slug", slug}, {"data", data}}.dump()));
    }
    if (action == "delete-news") {
      return Forward(host().ApiRequest(
          "POST", "/api/v2/news/" + slug + "/delete", "{}"));
    }
    if (action == "home-add") {
      return Forward(host().ApiRequest(
          "POST", "/api/v2/home-news/add", json{{"slug", slug}}.dump()));
    }
    if (action == "home-remove") {
      return Forward(host().ApiRequest(
          "POST", "/api/v2/home-news/remove", json{{"slug", slug}}.dump()));
    }
    return Fail("未知动作: " + action);
  }

 private:
  static std::string Fail(const std::string& message) {
    return json{{"ok", false}, {"message", message}}.dump();
  }

  static std::string Ok(const std::string& message) {
    return json{{"ok", true}, {"message", message}, {"refresh", "news"}}.dump();
  }

  static std::string Forward(const std::string& response) {
    const json parsed = json::parse(response, nullptr, false);
    if (parsed.is_discarded()) return Fail("服务端返回了无效的 JSON");
    if (parsed.is_object() && parsed.contains("error")) {
      const auto& error = parsed.at("error");
      return Fail(error.value("message", error.value("code", std::string("请求失败"))));
    }
    return Ok("完成");
  }
};

}  // namespace

SPARK_PLUGIN_EXPORT(NewsAdminPlugin)

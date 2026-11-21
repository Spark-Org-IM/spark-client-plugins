// Optional plugin: forum management through the host's forum proxy.
//
// It never sees the forum token. Every request is sent by the client, which
// adds the token and enforces the same origin as the configured forum.
#include <cctype>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "plugin/sdk.h"

namespace {

using nlohmann::json;

std::string Trim(const std::string& text) {
  std::size_t begin = 0;
  std::size_t end = text.size();
  while (begin < end &&
         std::isspace(static_cast<unsigned char>(text[begin]))) {
    ++begin;
  }
  while (end > begin &&
         std::isspace(static_cast<unsigned char>(text[end - 1]))) {
    --end;
  }
  return text.substr(begin, end - begin);
}

// Flarum ids are numeric in practice; refuse anything that could change the
// request path.
bool SafeId(const std::string& id) {
  if (id.empty()) return false;
  for (char c : id) {
    const bool ok = std::isalnum(static_cast<unsigned char>(c)) || c == '-' ||
                    c == '_';
    if (!ok) return false;
  }
  return true;
}

std::vector<std::string> SplitTags(const std::string& text) {
  std::vector<std::string> tags;
  std::string current;
  for (char c : text) {
    if (c == ',' || std::isspace(static_cast<unsigned char>(c))) {
      const std::string value = Trim(current);
      if (!value.empty()) tags.push_back(value);
      current.clear();
    } else {
      current.push_back(c);
    }
  }
  const std::string value = Trim(current);
  if (!value.empty()) tags.push_back(value);
  return tags;
}

class ForumManagePlugin : public spark::plugin::Plugin {
 public:
  explicit ForumManagePlugin(const spark_host_api* host)
      : spark::plugin::Plugin(host) {}

  std::string Id() const override { return "manage-forum"; }
  std::string Name() const override { return "论坛管理"; }
  std::string Version() const override { return "0.1.0"; }

  std::string ActionsJson() const override {
    json actions = json::array();
    actions.push_back({
        {"id", "create-discussion"},
        {"title", "发布主题"},
        {"description", "新建论坛主题；标签填标签 ID，逗号分隔，可留空"},
        {"fields",
         json::array({
             {{"key", "title"}, {"label", "标题"}, {"type", "text"},
              {"required", true}},
             {{"key", "content"},
              {"label", "正文"},
              {"type", "multiline"},
              {"required", true}},
             {{"key", "tags"},
              {"label", "标签 ID（逗号分隔，可留空）"},
              {"type", "text"}},
         })},
    });
    actions.push_back({
        {"id", "reply-discussion"},
        {"title", "回复主题"},
        {"description", "在指定主题下发送回复"},
        {"fields",
         json::array({
             {{"key", "discussion"},
              {"label", "主题 ID"},
              {"type", "text"},
              {"required", true}},
             {{"key", "content"},
              {"label", "回复内容"},
              {"type", "multiline"},
              {"required", true}},
         })},
    });
    actions.push_back({
        {"id", "update-discussion"},
        {"title", "修改主题"},
        {"description", "修改标题、置顶、锁定或隐藏；留空的字段保持不变"},
        {"fields",
         json::array({
             {{"key", "discussion"},
              {"label", "主题 ID"},
              {"type", "text"},
              {"required", true}},
             {{"key", "title"}, {"label", "新标题（可留空）"}, {"type", "text"}},
             {{"key", "sticky"},
              {"label", "置顶（true / false，可留空）"},
              {"type", "text"}},
             {{"key", "locked"},
              {"label", "锁定（true / false，可留空）"},
              {"type", "text"}},
             {{"key", "hidden"},
              {"label", "隐藏（true / false，可留空）"},
              {"type", "text"}},
         })},
    });
    actions.push_back({
        {"id", "delete-discussion"},
        {"title", "删除主题"},
        {"description", "删除指定主题；需要论坛权限"},
        {"fields",
         json::array({
             {{"key", "discussion"},
              {"label", "主题 ID"},
              {"type", "text"},
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

    if (action == "create-discussion") return CreateDiscussion(values);
    if (action == "reply-discussion") return ReplyDiscussion(values);
    if (action == "update-discussion") return UpdateDiscussion(values);
    if (action == "delete-discussion") return DeleteDiscussion(values);
    return Fail("未知动作: " + action);
  }

 private:
  std::string CreateDiscussion(const json& values) const {
    const std::string title = Trim(values.value("title", std::string{}));
    const std::string content = values.value("content", std::string{});
    if (title.empty()) return Fail("标题不能为空");
    if (Trim(content).empty()) return Fail("正文不能为空");

    json tags = json::array();
    for (const std::string& id :
         SplitTags(values.value("tags", std::string{}))) {
      if (!SafeId(id)) return Fail("标签 ID 不合法: " + id);
      tags.push_back({{"type", "tags"}, {"id", id}});
    }

    const json body = {
        {"data",
         {{"type", "discussions"},
          {"attributes", {{"title", title}, {"content", content}}},
          {"relationships", {{"tags", {{"data", tags}}}}}}},
    };
    return Forward(
        host().ForumRequest("POST", "/api/discussions", body.dump()));
  }

  std::string ReplyDiscussion(const json& values) const {
    const std::string id = Trim(values.value("discussion", std::string{}));
    const std::string content = values.value("content", std::string{});
    if (!SafeId(id)) return Fail("主题 ID 不合法");
    if (Trim(content).empty()) return Fail("回复内容不能为空");

    const json body = {
        {"data",
         {{"type", "posts"},
          {"attributes", {{"content", content}}},
          {"relationships",
           {{"discussion",
             {{"data", {{"type", "discussions"}, {"id", id}}}}}}}}},
    };
    return Forward(host().ForumRequest("POST", "/api/posts", body.dump()));
  }

  std::string UpdateDiscussion(const json& values) const {
    const std::string id = Trim(values.value("discussion", std::string{}));
    if (!SafeId(id)) return Fail("主题 ID 不合法");

    json attributes = json::object();
    const std::string title = Trim(values.value("title", std::string{}));
    if (!title.empty()) attributes["title"] = title;

    const char* flags[] = {"sticky", "locked", "hidden"};
    for (const char* key : flags) {
      const std::string raw = Trim(values.value(key, std::string{}));
      if (raw.empty()) continue;
      bool value = false;
      if (raw == "true" || raw == "1" || raw == "yes" || raw == "on") {
        value = true;
      } else if (raw == "false" || raw == "0" || raw == "no" ||
                 raw == "off") {
        value = false;
      } else {
        return Fail(std::string(key) + " 需要 true 或 false");
      }
      attributes[key] = value;
    }
    if (attributes.empty()) return Fail("没有需要修改的字段");

    const json body = {
        {"data",
         {{"type", "discussions"}, {"id", id}, {"attributes", attributes}}},
    };
    return Forward(host().ForumRequest(
        "PATCH", "/api/discussions/" + id, body.dump()));
  }

  std::string DeleteDiscussion(const json& values) const {
    const std::string id = Trim(values.value("discussion", std::string{}));
    if (!SafeId(id)) return Fail("主题 ID 不合法");
    return Forward(host().ForumRequest("DELETE", "/api/discussions/" + id, ""));
  }

  static std::string Fail(const std::string& message) {
    return json{{"ok", false}, {"message", message}}.dump();
  }

  static std::string Ok(const std::string& message) {
    return json{{"ok", true}, {"message", message}, {"refresh", "forum"}}.dump();
  }

  static std::string Forward(const std::string& response) {
    const json parsed = json::parse(response, nullptr, false);
    if (parsed.is_discarded()) return Fail("服务端返回了无效的 JSON");
    if (parsed.is_object() && parsed.contains("error")) {
      const auto& error = parsed.at("error");
      return Fail(
          error.value("message", error.value("code", std::string("请求失败"))));
    }
    return Ok("完成");
  }
};

}  // namespace

SPARK_PLUGIN_EXPORT(ForumManagePlugin)

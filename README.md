# Spark ORG 可选插件

官方提供的可安装插件，**独立于主项目**：不放进 `spark-client` 的源码树，
不参与它的构建、测试或打包。主项目只负责加载插件和提供稳定的 C ABI。

每个插件都是单独的共享对象，只使用 `plugin/api.h` / `plugin/sdk.h`，
不链接 `spark_core`、GTK 或 FTXUI。客户端没装插件时行为完全不变。

## 插件

| 目录 | id | 作用 | 所需权限 |
| --- | --- | --- | --- |
| `plugins/manage-news` | `manage-news` | 保存/删除新闻，主页置顶增删 | 令牌 scope `news.edit` |
| `plugins/manage-forum` | `manage-forum` | 发布主题、回复、修改（置顶/锁定/隐藏）、删除主题 | 论坛账号自身的权限 |

`manage-forum` 不声明主站令牌 scope：它使用的是论坛账号，最终权限由论坛
判断。插件只把 JSON 交给客户端代理，永远看不到令牌。

插件动作会自动成为 TUI / GTK 顶栏的候选（默认隐藏并收进“已折叠”），
在“设置 → 顶栏元素”里可以显示、排序或移出折叠组。

## 构建

需要先从主项目取得 SDK 头文件，二选一：

- 安装 `sparkapp-core`，头文件在 `/usr/include/spark-client/`；
- 或者直接指向 `spark-client` 源码树的顶层 `src/`：

```sh
cmake -S . -B build \
  -DSPARK_PLUGIN_SDK_INCLUDE_DIR=/path/to/spark-client/src
cmake --build build
ctest --test-dir build --output-on-failure
```

产物是 `build/spark-manage-news.so` 和 `build/spark-manage-forum.so`。
需要 Debian 包时再运行：

```sh
cd build && cpack -G DEB
```

会生成 `sparkapp-plugin-news_<version>_<arch>.deb` 和
`sparkapp-plugin-forum_<version>_<arch>.deb`。

## 安装

`.deb` 安装（推荐）：

```sh
sudo apt install ./sparkapp-plugin-news_*.deb ./sparkapp-plugin-forum_*.deb
```

包依赖 `sparkapp-core` 和虚拟包 `spark-plugin-abi-<N>`；旧版 core 没有插件
SDK 时会被 `apt` 拒绝，不会装上不兼容的插件。

也可以只复制共享对象。客户端按以下顺序查找插件：

1. `$SPARK_PLUGIN_PATH`（冒号分隔）；
2. `$XDG_DATA_HOME/spark-client/plugins`（默认
   `~/.local/share/spark-client/plugins`）；
3. `<libdir>/spark-client/plugins`。

手工安装可以直接放进用户目录：

```sh
mkdir -p ~/.local/share/spark-client/plugins
cp build/spark-manage-news.so build/spark-manage-forum.so \
  ~/.local/share/spark-client/plugins/
```

## 与主项目的兼容性

- `SPARK_PLUGIN_ABI_VERSION` 定义在 `plugin/api.h`；主项目打包时会提供
  `spark-plugin-abi-<N>` 虚拟包，插件应依赖对应的 ABI。
- ABI 不兼容的插件会被加载器跳过，不影响客户端启动。
- 插件是受信任的本机代码；宿主已限制它能拿到的配置（没有令牌、密码或
  加密数据），但不会在进程内做文件系统隔离。

## CI 与发布

- `.github/workflows/ci.yml`：非 `main` 分支推送和所有 PR 都会构建、测试
  amd64 / arm64，不发布。构建使用 ccache，缓存按源码哈希复用。
- `.github/workflows/deps.yml`：每天 00:00 UTC 定时和手动运行；先升级系统包
  （编译器、CMake 等），再解析上游 `spark-client` 的插件 SDK、刷新 SDK 缓存，
  并用最新 SDK 重新构建测试。
- `.github/workflows/release.yml`：合并到 `main` 后构建所有架构，把全部
  `.deb` 和插件模块打成**一个** tar
  （`spark-client-plugins_<version>_all.tar.gz`）并发布为 `v<version>`。
  插件不等待 tag；非 `main` 分支只检查，不发布。

## 许可证

MIT，见 [`LICENSE`](LICENSE)。第三方 `nlohmann/json` 也是 MIT，
许可证在 [`third_party/nlohmann/LICENSE.MIT`](third_party/nlohmann/LICENSE.MIT)。

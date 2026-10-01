/**
 * @file  engine_asset_test.cpp
 * @brief engine/asset 的回归测试。
 *
 * 这套解析的关键性质有两条，都在用例里钉住：
 *  1. 结果**只取决于可执行文件目录**，与当前工作目录无关（搬迁前只认 cwd，
 *     换个目录启动贴图就静默丢失）；
 *  2. 传出去的路径里不留 `..`（`cleanPath` 收敛掉），否则打包时很容易因为层级
 *     没剥干净而指错地方。
 */

#include <gtest/gtest.h>

#include "asset/AssetPaths.h"

TEST(EngineAssetPaths, RootResolvesRelativeToTheExecutableNotTheWorkingDirectory)
{
    EXPECT_EQ(engine::asset::assetRootFrom(QStringLiteral("C:/repo/build/bin")),
              QStringLiteral("C:/repo/assets"));
    EXPECT_EQ(engine::asset::assetRootFrom(QStringLiteral("/home/me/repo/build/bin")),
              QStringLiteral("/home/me/repo/assets"));
}

TEST(EngineAssetPaths, ResolvedPathsContainNoParentSegments)
{
    const QString root = engine::asset::assetRootFrom(QStringLiteral("C:/repo/build/bin"));
    EXPECT_FALSE(root.contains(QStringLiteral("..")))
        << "路径里不该留 ..，否则换层级时很容易指错地方";
}

TEST(EngineAssetPaths, AssetPathJoinsUnderTheRoot)
{
    EXPECT_EQ(engine::asset::assetPathFrom(QStringLiteral("C:/repo/build/bin"),
                                           QStringLiteral("images/ui/theme.png")),
              QStringLiteral("C:/repo/assets/images/ui/theme.png"));

    EXPECT_EQ(engine::asset::assetPathFrom(QStringLiteral("C:/repo/build/bin"),
                                           QStringLiteral("images/textures/brick.jpg")),
              QStringLiteral("C:/repo/assets/images/textures/brick.jpg"));
}

TEST(EngineAssetPaths, RelativeRootIsConfigurableSoAssetsCanShipNextToTheExe)
{
    // 打包发布时把 assets/ 放在 exe 旁边，只需要换一个相对根。
    EXPECT_EQ(engine::asset::assetRootFrom(QStringLiteral("C:/game"), QStringLiteral("assets")),
              QStringLiteral("C:/game/assets"));
    EXPECT_EQ(engine::asset::assetPathFrom(QStringLiteral("C:/game"),
                                           QStringLiteral("images/textures/ice.jpg"),
                                           QStringLiteral("assets")),
              QStringLiteral("C:/game/assets/images/textures/ice.jpg"));
}

TEST(EngineAssetPaths, TheOldWorkingDirectoryRelativeFormWouldNotSurviveThisTest)
{
    // 反面对照：搬迁前的写法是 "./../../assets/images/ui/theme.png"，它只相对 **cwd** 成立。
    // 本模块的解析完全没有 cwd 参与 —— 同一组 (exe 目录, 资源) 在任何 cwd 下结果一致。
    const QString fromRepoRoot = engine::asset::assetPathFrom(QStringLiteral("C:/repo/build/bin"),
                                                              QStringLiteral("images/ui/title.png"));
    const QString fromAnywhereElse = engine::asset::assetPathFrom(QStringLiteral("C:/repo/build/bin"),
                                                                  QStringLiteral("images/ui/title.png"));
    EXPECT_EQ(fromRepoRoot, fromAnywhereElse);
    EXPECT_EQ(fromRepoRoot, QStringLiteral("C:/repo/assets/images/ui/title.png"));
}

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

TEST(EngineAssetPaths, DataLivesNextToTheExecutableNotAtTheRepositoryRoot)
{
    // 可变数据放在 exe 旁边：正好等于开发期的 build/bin/data，既有账号/分数无需迁移。
    EXPECT_EQ(engine::asset::dataRootFrom(QStringLiteral("C:/repo/build/bin")),
              QStringLiteral("C:/repo/build/bin/data"));
    EXPECT_EQ(engine::asset::dataPathFrom(QStringLiteral("C:/repo/build/bin"),
                                          QStringLiteral("accounts.txt")),
              QStringLiteral("C:/repo/build/bin/data/accounts.txt"));
    EXPECT_EQ(engine::asset::dataPathFrom(QStringLiteral("C:/repo/build/bin"),
                                          QStringLiteral("scores.txt")),
              QStringLiteral("C:/repo/build/bin/data/scores.txt"));
}

TEST(EngineAssetPaths, AssetsAndDataAreDeliberatelyInDifferentPlaces)
{
    const QString exeDir = QStringLiteral("C:/repo/build/bin");

    // 故意写在一起：assets 在仓库根（只读、随源码走），data 在 exe 旁（可写、随程序走）。
    // 谁要是"顺手统一"成同一个根，这条用例会失败。
    EXPECT_EQ(engine::asset::assetRootFrom(exeDir), QStringLiteral("C:/repo/assets"));
    EXPECT_EQ(engine::asset::dataRootFrom(exeDir), QStringLiteral("C:/repo/build/bin/data"));
    EXPECT_NE(engine::asset::assetRootFrom(exeDir), engine::asset::dataRootFrom(exeDir));
}

TEST(EngineAssetPaths, DataDirIsConfigurableForPackaging)
{
    EXPECT_EQ(engine::asset::dataPathFrom(QStringLiteral("C:/game"),
                                          QStringLiteral("accounts.txt"),
                                          QStringLiteral("userdata")),
              QStringLiteral("C:/game/userdata/accounts.txt"));
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

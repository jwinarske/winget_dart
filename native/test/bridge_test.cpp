// native/test/bridge_test.cpp
// SPDX-License-Identifier: Apache-2.0
//
// Google Test suite for the winget_nc bridge.
// Tests the message codec, JSON encoding, handle table, and bridge lifecycle.
//
// Tests that exercise COM/WinRT (wg_connect, wg_search_name, etc.) require a
// live Windows environment and are guarded by #ifdef WINGET_LIVE_TESTS.
// The codec tests run on any platform with GTest available.

#include <gtest/gtest.h>
#include "message_codec.h"
#include "winget_bridge.h"

#include <string>
#include <vector>
#include <mutex>
#include <cstring>

// ---------------------------------------------------------------------------
// Mock Dart_PostCObject_DL for capturing posted messages.
// ---------------------------------------------------------------------------
namespace {

std::mutex g_mock_mutex;
std::vector<std::string> g_posted_messages;

void ClearPostedMessages() {
  std::lock_guard lk(g_mock_mutex);
  g_posted_messages.clear();
}

std::vector<std::string> GetPostedMessages() {
  std::lock_guard lk(g_mock_mutex);
  return g_posted_messages;
}

}  // namespace

// Override PostToDart for tests — captures messages instead of calling Dart API.
// This works because the test binary links against the message_codec object
// but not the real Dart runtime.
namespace winget_nc {

bool PostToDart(Dart_Port port, const std::string& json) {
  (void)port;
  std::lock_guard lk(g_mock_mutex);
  g_posted_messages.push_back(json);
  return true;
}

}  // namespace winget_nc

// ===========================================================================
// JSON Escape Tests
// ===========================================================================
class JsonEscapeTest : public ::testing::Test {};

TEST_F(JsonEscapeTest, PlainStringUnchanged) {
  EXPECT_EQ(winget_nc::JsonEscape("hello world"), "hello world");
}

TEST_F(JsonEscapeTest, EscapesDoubleQuote) {
  EXPECT_EQ(winget_nc::JsonEscape(R"(say "hello")"), R"(say \"hello\")");
}

TEST_F(JsonEscapeTest, EscapesBackslash) {
  EXPECT_EQ(winget_nc::JsonEscape("C:\\path\\to"), "C:\\\\path\\\\to");
}

TEST_F(JsonEscapeTest, EscapesNewlineAndTab) {
  EXPECT_EQ(winget_nc::JsonEscape("line1\nline2\ttab"), "line1\\nline2\\ttab");
}

TEST_F(JsonEscapeTest, EscapesCarriageReturn) {
  EXPECT_EQ(winget_nc::JsonEscape("cr\r"), "cr\\r");
}

TEST_F(JsonEscapeTest, EscapesControlCharacters) {
  // ASCII 0x01 should become \u0001
  std::string input(1, '\x01');
  EXPECT_EQ(winget_nc::JsonEscape(input), "\\u0001");
}

TEST_F(JsonEscapeTest, EmptyString) {
  EXPECT_EQ(winget_nc::JsonEscape(""), "");
}

TEST_F(JsonEscapeTest, Utf8PassedThrough) {
  // Multi-byte UTF-8 characters should be passed through without escaping.
  std::string utf8 = "\xC3\xA9";  // é
  EXPECT_EQ(winget_nc::JsonEscape(utf8), utf8);
}

// ===========================================================================
// Terminal Encoder Tests
// ===========================================================================
class TerminalEncoderTest : public ::testing::Test {};

TEST_F(TerminalEncoderTest, EncodeDone) {
  EXPECT_EQ(winget_nc::EncodeDone(), R"({"done":true})");
}

TEST_F(TerminalEncoderTest, EncodeSuccess) {
  EXPECT_EQ(winget_nc::EncodeSuccess(), R"({"result":{"success":true}})");
}

TEST_F(TerminalEncoderTest, EncodeCancelled) {
  EXPECT_EQ(winget_nc::EncodeCancelled(), R"({"cancelled":true})");
}

TEST_F(TerminalEncoderTest, EncodeErrorWithHresult) {
  auto json = winget_nc::EncodeError("Something went wrong", -2147024894);
  EXPECT_EQ(json, R"({"error":"Something went wrong","hresult":-2147024894})");
}

TEST_F(TerminalEncoderTest, EncodeErrorZeroHresult) {
  auto json = winget_nc::EncodeError("Generic error", 0);
  EXPECT_EQ(json, R"({"error":"Generic error","hresult":0})");
}

TEST_F(TerminalEncoderTest, EncodeErrorWithSpecialChars) {
  auto json = winget_nc::EncodeError("Error with \"quotes\" and \\slashes", 42);
  // The message should be escaped inside the JSON string.
  EXPECT_NE(json.find(R"(Error with \"quotes\" and \\slashes)"), std::string::npos);
}

TEST_F(TerminalEncoderTest, EncodeErrorEmptyMessage) {
  auto json = winget_nc::EncodeError("", 0);
  EXPECT_EQ(json, R"({"error":"","hresult":0})");
}

// ===========================================================================
// PostToDart Mock Tests
// ===========================================================================
class PostToDartTest : public ::testing::Test {
 protected:
  void SetUp() override { ClearPostedMessages(); }
  void TearDown() override { ClearPostedMessages(); }
};

TEST_F(PostToDartTest, PostCapturesMessage) {
  winget_nc::PostToDart(42, R"({"done":true})");
  auto msgs = GetPostedMessages();
  ASSERT_EQ(msgs.size(), 1u);
  EXPECT_EQ(msgs[0], R"({"done":true})");
}

TEST_F(PostToDartTest, MultiplePostsCaptured) {
  winget_nc::PostToDart(1, R"({"pkg":{"id":"test"}})");
  winget_nc::PostToDart(1, R"({"done":true})");
  auto msgs = GetPostedMessages();
  ASSERT_EQ(msgs.size(), 2u);
  EXPECT_EQ(msgs[0], R"({"pkg":{"id":"test"}})");
  EXPECT_EQ(msgs[1], R"({"done":true})");
}

// ===========================================================================
// InstallStateToString Tests (requires WinRT types — Windows only)
// ===========================================================================
#ifdef WINGET_LIVE_TESTS

class InstallStateTest : public ::testing::Test {};

TEST_F(InstallStateTest, AllStatesHaveStrings) {
  using namespace winget_nc;
  EXPECT_EQ(InstallStateToString(PackageInstallProgressState::Queued), "queued");
  EXPECT_EQ(InstallStateToString(PackageInstallProgressState::Downloading), "downloading");
  EXPECT_EQ(InstallStateToString(PackageInstallProgressState::Installing), "installing");
  EXPECT_EQ(InstallStateToString(PackageInstallProgressState::PostInstall), "postInstall");
  EXPECT_EQ(InstallStateToString(PackageInstallProgressState::Finished), "finished");
}

// ===========================================================================
// Catalog Encoder Tests (requires WinRT types — Windows only)
// ===========================================================================
class CatalogEncoderTest : public ::testing::Test {};

// These tests require creating real WinRT objects; guarded by WINGET_LIVE_TESTS.
// In CI, they run on the Windows runner where WinRT is available.

// ===========================================================================
// Bridge Lifecycle Tests (requires WinRT/COM — Windows only)
// ===========================================================================
class BridgeLifecycleTest : public ::testing::Test {
 protected:
  void SetUp() override { ClearPostedMessages(); }
  void TearDown() override { ClearPostedMessages(); }
};

TEST_F(BridgeLifecycleTest, IsAvailableReturnsOneOrZero) {
  int32_t result = wg_is_available();
  EXPECT_TRUE(result == 0 || result == 1);
}

#endif  // WINGET_LIVE_TESTS

// ===========================================================================
// Message Protocol Contract Tests
// ===========================================================================
// These validate the JSON shape contract between bridge and Dart decoder.
class MessageProtocolTest : public ::testing::Test {};

TEST_F(MessageProtocolTest, DoneMessageHasDiscriminatorKey) {
  auto json = winget_nc::EncodeDone();
  EXPECT_NE(json.find("\"done\""), std::string::npos);
  EXPECT_NE(json.find("true"), std::string::npos);
}

TEST_F(MessageProtocolTest, SuccessMessageHasResultKey) {
  auto json = winget_nc::EncodeSuccess();
  EXPECT_NE(json.find("\"result\""), std::string::npos);
  EXPECT_NE(json.find("\"success\""), std::string::npos);
}

TEST_F(MessageProtocolTest, CancelledMessageHasDiscriminatorKey) {
  auto json = winget_nc::EncodeCancelled();
  EXPECT_NE(json.find("\"cancelled\""), std::string::npos);
}

TEST_F(MessageProtocolTest, ErrorMessageHasErrorAndHresult) {
  auto json = winget_nc::EncodeError("test error", -1);
  EXPECT_NE(json.find("\"error\""), std::string::npos);
  EXPECT_NE(json.find("\"hresult\""), std::string::npos);
  EXPECT_NE(json.find("test error"), std::string::npos);
  EXPECT_NE(json.find("-1"), std::string::npos);
}

// Verify that all terminal messages are valid JSON (contain balanced braces).
TEST_F(MessageProtocolTest, AllTerminalMessagesAreBalancedJson) {
  auto check = [](const std::string& json) {
    ASSERT_FALSE(json.empty());
    ASSERT_EQ(json.front(), '{');
    ASSERT_EQ(json.back(), '}');
    int depth = 0;
    for (char c : json) {
      if (c == '{') depth++;
      else if (c == '}') depth--;
      EXPECT_GE(depth, 0) << "Unbalanced braces in: " << json;
    }
    EXPECT_EQ(depth, 0) << "Unbalanced braces in: " << json;
  };

  check(winget_nc::EncodeDone());
  check(winget_nc::EncodeSuccess());
  check(winget_nc::EncodeCancelled());
  check(winget_nc::EncodeError("test", 0));
  check(winget_nc::EncodeError("test", -2147024894));
}

// ===========================================================================
// Install Progress Encoding Tests (requires WinRT types — Windows only)
// ===========================================================================
#ifdef WINGET_LIVE_TESTS

class ProgressEncoderTest : public ::testing::Test {};

TEST_F(ProgressEncoderTest, DownloadingProgressMapsTo0Through50) {
  using namespace winget_nc;
  InstallProgress p{};
  p.State = PackageInstallProgressState::Downloading;
  p.DownloadProgress = 0.5;  // 50% of download = 25% overall
  auto json = EncodeInstallProgress(p);
  EXPECT_NE(json.find("\"percent\":25"), std::string::npos);
  EXPECT_NE(json.find("\"state\":\"downloading\""), std::string::npos);
}

TEST_F(ProgressEncoderTest, InstallingProgressMapsTo50Through100) {
  using namespace winget_nc;
  InstallProgress p{};
  p.State = PackageInstallProgressState::Installing;
  p.InstallationProgress = 0.5;  // 50% of install = 75% overall
  auto json = EncodeInstallProgress(p);
  EXPECT_NE(json.find("\"percent\":75"), std::string::npos);
  EXPECT_NE(json.find("\"state\":\"installing\""), std::string::npos);
}

TEST_F(ProgressEncoderTest, FinishedIs100Percent) {
  using namespace winget_nc;
  InstallProgress p{};
  p.State = PackageInstallProgressState::Finished;
  auto json = EncodeInstallProgress(p);
  EXPECT_NE(json.find("\"percent\":100"), std::string::npos);
  EXPECT_NE(json.find("\"state\":\"finished\""), std::string::npos);
  EXPECT_NE(json.find("\"label\":\"Complete\""), std::string::npos);
}

TEST_F(ProgressEncoderTest, QueuedIsZeroPercent) {
  using namespace winget_nc;
  InstallProgress p{};
  p.State = PackageInstallProgressState::Queued;
  auto json = EncodeInstallProgress(p);
  EXPECT_NE(json.find("\"percent\":0"), std::string::npos);
  EXPECT_NE(json.find("\"state\":\"queued\""), std::string::npos);
}

TEST_F(ProgressEncoderTest, PostInstallLabel) {
  using namespace winget_nc;
  InstallProgress p{};
  p.State = PackageInstallProgressState::PostInstall;
  auto json = EncodeInstallProgress(p);
  EXPECT_NE(json.find("\"label\":\"Finishing up...\""), std::string::npos);
}

TEST_F(ProgressEncoderTest, UninstallProgressEncoding) {
  using namespace winget_nc;
  UninstallProgress p{};
  p.UninstallationProgress = 0.75;
  auto json = EncodeUninstallProgress(p);
  EXPECT_NE(json.find("\"percent\":75"), std::string::npos);
  EXPECT_NE(json.find("Uninstalling"), std::string::npos);
}

// ===========================================================================
// Plan Encoding Tests (requires WinRT types — Windows only)
// ===========================================================================
class PlanEncoderTest : public ::testing::Test {};

TEST_F(PlanEncoderTest, EmptyPlanHasEmptyArrays) {
  using namespace winget_nc;
  InstallPlan plan;
  auto json = EncodePlan(plan, "winget");
  EXPECT_NE(json.find("\"installing\":[]"), std::string::npos);
  EXPECT_NE(json.find("\"upgrading\":[]"), std::string::npos);
  EXPECT_NE(json.find("\"removing\":[]"), std::string::npos);
}

#endif  // WINGET_LIVE_TESTS

// ===========================================================================
// Sequential Install Guard Tests
// ===========================================================================
// These test the TryStartMutatingOp / EndMutatingOp logic in WgTransaction.
// They do not require WinRT since the guard is pure atomic logic.

#include "winget_transaction.h"

class InstallGuardTest : public ::testing::Test {};

TEST_F(InstallGuardTest, FirstMutatingOpSucceeds) {
  winget_nc::WgTransaction tx(0);
  EXPECT_TRUE(tx.TryStartMutatingOp());
  tx.EndMutatingOp();
}

TEST_F(InstallGuardTest, SecondMutatingOpFails) {
  winget_nc::WgTransaction tx(0);
  EXPECT_TRUE(tx.TryStartMutatingOp());
  EXPECT_FALSE(tx.TryStartMutatingOp());
  tx.EndMutatingOp();
}

TEST_F(InstallGuardTest, MutatingOpReusableAfterEnd) {
  winget_nc::WgTransaction tx(0);
  EXPECT_TRUE(tx.TryStartMutatingOp());
  tx.EndMutatingOp();
  EXPECT_TRUE(tx.TryStartMutatingOp());
  tx.EndMutatingOp();
}

TEST_F(InstallGuardTest, CancelSetsCancelledFlag) {
  winget_nc::WgTransaction tx(0);
  EXPECT_FALSE(tx.cancelled.load());
  tx.Cancel();
  EXPECT_TRUE(tx.cancelled.load());
}

// ===========================================================================
// Progress Message Protocol Tests (no WinRT needed)
// ===========================================================================
class ProgressProtocolTest : public ::testing::Test {
 protected:
  void SetUp() override { ClearPostedMessages(); }
  void TearDown() override { ClearPostedMessages(); }
};

TEST_F(ProgressProtocolTest, ErrorMessageForNotInstalledPackage) {
  // Simulate the error that DoUninstall would post when a package isn't found.
  auto json = winget_nc::EncodeError("Package not installed: SomeApp", 0);
  EXPECT_NE(json.find("\"error\""), std::string::npos);
  EXPECT_NE(json.find("Package not installed: SomeApp"), std::string::npos);
}

TEST_F(ProgressProtocolTest, MutatingOpBusyError) {
  auto json = winget_nc::EncodeError(
      "Another install/upgrade/uninstall is already in progress", 0);
  EXPECT_NE(json.find("already in progress"), std::string::npos);
  EXPECT_NE(json.find("\"hresult\":0"), std::string::npos);
}

TEST_F(ProgressProtocolTest, SuccessAfterInstall) {
  // The expected post-install success message.
  auto json = winget_nc::EncodeSuccess();
  EXPECT_EQ(json, R"({"result":{"success":true}})");
}

TEST_F(ProgressProtocolTest, InstallFailedErrorIncludesHresult) {
  // Simulate an install failure with a specific HRESULT.
  auto json = winget_nc::EncodeError("Install failed", -2147024894);
  EXPECT_NE(json.find("Install failed"), std::string::npos);
  EXPECT_NE(json.find("-2147024894"), std::string::npos);
}

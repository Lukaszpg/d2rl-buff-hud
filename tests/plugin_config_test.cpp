#include "core/plugin_config.hpp"

#include <cstdio>

using BuffPanel::Core::ParsePluginConfig;

int main() {
    int failures = 0;
    auto check = [&](bool condition, const char* what) {
        if (!condition) {
            std::printf("FAIL: %s\n", what);
            ++failures;
        }
    };
    check(ParsePluginConfig(nullptr).hideWithPanels, "no config keeps hide_with_panels on");
    check(ParsePluginConfig("").hideWithPanels, "an empty config keeps hide_with_panels on");
    check(!ParsePluginConfig("[buff_panel]\nhide_with_panels = false\n").hideWithPanels, "hide_with_panels = false");
    check(!ParsePluginConfig("# c\r\n[buff_panel]\r\n  hide_with_panels=false   # off\r\n").hideWithPanels,
        "CRLF, comments and spacing");
    check(ParsePluginConfig("[buff_panel]\nhide_with_panels = true\n").hideWithPanels, "hide_with_panels = true");
    check(ParsePluginConfig("[other]\nhide_with_panels = false\n").hideWithPanels, "another section does not count");
    check(ParsePluginConfig("[buff_panel]\nhide_with_panels = 0\n").hideWithPanels, "a bad value keeps the default");
    check(ParsePluginConfig("[buff_panel]\nhide_with_panels_x = false\n").hideWithPanels, "another key does not count");
    check(ParsePluginConfig("hide_with_panels = false\n").hideWithPanels, "a key outside any section does not count");
    check(ParsePluginConfig(nullptr).enabled, "no config keeps the panel on");
    check(ParsePluginConfig("").enabled, "an empty config keeps the panel on");
    check(!ParsePluginConfig("[buff_panel]\nenabled = false\n").enabled, "enabled = false turns it off");
    check(!ParsePluginConfig("# c\r\n[buff_panel]\r\n  enabled=false   # off\r\n").enabled, "enabled: CRLF, comments and spacing");
    check(ParsePluginConfig("[buff_panel]\nenabled = true\n").enabled, "enabled = true");
    check(ParsePluginConfig("[other]\nenabled = false\n").enabled, "enabled: another section does not count");
    check(ParsePluginConfig("[buff_panel]\nenabled = no\n").enabled, "enabled: a bad value keeps the default");
    check(ParsePluginConfig("[buff_panel]\nenabled_x = false\n").enabled, "enabled: another key does not count");
    check(ParsePluginConfig("enabled = false\n").enabled, "enabled: a key outside any section does not count");
    check(!ParsePluginConfig("[buff_panel]\nenabled = false\nhide_with_panels = false\n").hideWithPanels, "both keys in one file");
    check(ParsePluginConfig("[buff_panel]\nenabled = false\n").hideWithPanels, "enabled does not change hide_with_panels");
    if (failures == 0) std::printf("PASS: plugin config\n");
    return failures == 0 ? 0 : 1;
}

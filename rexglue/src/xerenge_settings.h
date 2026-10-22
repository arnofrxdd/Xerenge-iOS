// Xerenge's settings in the game's own Settings screen (Driver Details >
// Settings): the pages, their rows, and xerenge.conf behind them.
//
// Page 0 is the game's screen with a Language row and three buttons added;
// pages 1-3 (Graphics, Hacks, Debug) are the same screen with our rows in
// place of the game's (settings_movie.cpp builds the movie for each).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace xerenge::settings {

// kPause: the race's pause menu's Options, with what changes without a restart.
// kAchievements: the list of the game's achievements, opened from Player File
// (DriverDetailsProfile) in the Stats screen's movie.
enum Page : int { kMain = 0, kGraphics = 1, kHacks = 2, kDebug = 3, kPause = 4, kAchievements = 5 };

// A row: a choice between options, or (no options) a line shown as it is.
struct Row {
  std::string label;
  std::vector<std::string> options;  // shown as the choice's values
  int id = 0;                        // XerengeGet(id), XerengeSet(id * 16 + option)
};

// The page the Settings screen shows next, and its title ("" for the game's).
Page CurrentPage();
void SetPage(Page page);
std::string PageTitle(Page page);

// The American release has one language: no Language row there.
void SetSingleLanguage(bool single);

// The rows of a page, with their current values read from xerenge.conf.
std::vector<Row> Rows(Page page);

// The option a row is at, and choosing one (written to xerenge.conf, and
// applied at once where the renderer can take it).
int Get(int id);
void Set(int id, int option);

// Whether a choice made since the last call needs the game started again.
bool TakeRestartNeeded();

// The achievements as Stats-like rows (achievements_movie in settings_movie.cpp):
// the name, the score, and LOCKED or the day it was earned; the earned first.
struct AchievementRow {
  std::string name;
  std::string score;
  std::string state;
  std::string description;  // what it is for once earned, how to earn it before
};
std::vector<AchievementRow> AchievementRows();

// What must be known before the renderer starts, from xerenge.conf (RenderDoc).
void ApplyAtStart();

}  // namespace xerenge::settings

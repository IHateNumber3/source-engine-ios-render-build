#include "cbase.h"
#include "player_voice_listener.h"

// Заглушки для кооперативной статистики и путей кампании Portal 2
int GetBluePlayerIndex() { return 1; }
int GetOrangePlayerIndex() { return 2; }
bool IsLevelComplete(int iBranch, int iLevel) { return false; }
void MarkMapComplete(const char *pszMapName) {}
void AddBranchLevelName(int iBranch, const char *pszName) {}
void AddCoopCreditsName(const char *pszName) {}
int GetCoopSectionIndex() { return 0; }
bool IsPlayerLevelComplete(int i1, int i2, int i3) { return false; }
int GetHighestActiveBranch() { return 0; }
int GetCoopBranchLevelIndex(int i) { return 0; }
void PrecacheMovie(char const *pszMovieName) {}

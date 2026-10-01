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

// Заглушка для голосового лизтенера, если он вызывается в правилах
static CPlayerVoiceListener s_PlayerVoiceListener;
CPlayerVoiceListener *PlayerVoiceListener()
{
    return &s_PlayerVoiceListener;
}

float CPlayerVoiceListener::GetPlayerSilenceDuration(int iPlayerIndex)
{
    return 0.0f;
}

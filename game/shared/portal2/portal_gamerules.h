//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Game rules for Portal (Adapted for Alien Swarm Engine)
//
//=============================================================================//

#ifdef PORTAL_MP

#include "portal_mp_gamerules.h" // Redirect to multiplayer gamerules in multiplayer builds

#else

#ifndef PORTAL_GAMERULES_H
#define PORTAL_GAMERULES_H

#ifdef _WIN32
#pragma once
#endif

#include "gamerules.h"
#include "hl2_gamerules.h"
#include "convar.h"

#ifdef CLIENT_DLL
	#define CPortalGameRules C_PortalGameRules
	#define CPortalGameRulesProxy C_PortalGameRulesProxy
	#include "steam/steam_api.h"
#endif

class CPortalGameRulesProxy : public CGameRulesProxy
{
public:
	DECLARE_CLASS( CPortalGameRulesProxy, CGameRulesProxy );
	DECLARE_NETWORKCLASS();
};

class CPortalGameRules : public CHalfLife2
{
public:
	// Исправлено: указан правильный базовый класс CHalfLife2
	DECLARE_CLASS( CPortalGameRules, CHalfLife2 );

	virtual bool	Init();
	
	virtual bool	ShouldCollide( int collisionGroup0, int collisionGroup1 );
	virtual bool	ShouldUseRobustRadiusDamage( CBaseEntity *pEntity );
	virtual void	RegisterScriptFunctions( void );

#ifndef CLIENT_DLL
	virtual bool	ShouldAutoAim( CBasePlayer *pPlayer, edict_t *target );
	virtual float	GetAutoAimScale( CBasePlayer *pPlayer );
#endif

#ifdef CLIENT_DLL
	virtual bool	IsBonusChallengeTimeBased( void );
	DECLARE_CLIENTCLASS();
#else
	DECLARE_SERVERCLASS();

	CPortalGameRules();
	virtual ~CPortalGameRules() {}

	virtual void			Think( void );

	virtual bool			ClientCommand( CBaseEntity *pEdict, const CCommand &args );
	virtual void			PlayerSpawn( CBasePlayer *pPlayer );

	virtual void			InitDefaultAIRelationships( void );
	virtual const char*		AIClassText( int classType );
	virtual const char*		GetGameDescription( void ) { return "Portal"; }

	// Ammo & Player
	virtual void			PlayerThink( CBasePlayer *pPlayer );
	virtual float			GetAmmoDamage( CBaseEntity *pAttacker, CBaseEntity *pVictim, int nAmmoType );

	virtual bool			ShouldBurningPropsEmitLight();
	bool					ShouldRemoveRadio( void );

public:
	virtual float			FlPlayerFallDamage( CBasePlayer *pPlayer );
	bool					MegaPhyscannonActive( void ) { return m_bMegaPhysgun; }

private:
	int						DefaultFOV( void ) { return 75; }
#endif

private:
	// Переменная состояния супер-гравипушки
	CNetworkVar( bool, m_bMegaPhysgun );
};

//-----------------------------------------------------------------------------
// Глобальный доступ к правилам игры Portal
//-----------------------------------------------------------------------------
inline CPortalGameRules* PortalGameRules()
{
	return static_cast<CPortalGameRules*>(g_pGameRules);
}

#endif // PORTAL_GAMERULES_H
#endif

//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Portal Game Rules implementation adapted for Alien Swarm Engine
//
//=============================================================================//

#include "cbase.h"
#include "portal_gamerules.h"
#include "ammodef.h"
#include "hl2_shareddefs.h"
#include "portal_shareddefs.h"

#ifdef CLIENT_DLL
#else
	#include "player.h"
	#include "game.h"
	#include "gamerules.h"
	#include "teamplay_gamerules.h"
	#include "portal_player.h"
	#include "globalstate.h"
	#include "ai_basenpc.h"
	#include "portal/weapon_physcannon.h"
	#include "props.h"		
	#include "datacache/imdlcache.h"	

	#include "achievementmgr.h"
	extern CAchievementMgr g_AchievementMgrPortal;
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

REGISTER_GAMERULES_CLASS( CPortalGameRules );

// В Alien Swarm таблицы отсылки маппятся с ясным указанием структуры CPortalGameRules
BEGIN_NETWORK_TABLE_NOBASE( CPortalGameRules, DT_PortalGameRules )
	#ifdef CLIENT_DLL
		RecvPropBool( RECVINFO( m_bMegaPhysgun ) ),
	#else
		SendPropBool( SENDINFO( m_bMegaPhysgun ) ),
	#endif
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( portal_gamerules, CPortalGameRulesProxy );
IMPLEMENT_NETWORKCLASS_ALIASED( PortalGameRulesProxy, DT_PortalGameRulesProxy )

#ifdef CLIENT_DLL
	void RecvProxy_PortalGameRules( const RecvProp *pProp, void **pOut, void *pData, int objectID )
	{
		CPortalGameRules *pRules = PortalGameRules();
		Assert( pRules );
		*pOut = pRules;
	}

	BEGIN_RECV_TABLE( CPortalGameRulesProxy, DT_PortalGameRulesProxy )
		RecvPropDataTable( "portal_gamerules_data", 0, 0, &REFERENCE_RECV_TABLE( DT_PortalGameRules ), RecvProxy_PortalGameRules )
	END_RECV_TABLE()
#else
	void* SendProxy_PortalGameRules( const SendProp *pProp, const void *pStructBase, const void *pData, CSendProxyRecipients *pRecipients, int objectID )
	{
		CPortalGameRules *pRules = PortalGameRules();
		Assert( pRules );
		pRecipients->SetAllRecipients();
		return pRules;
	}

	BEGIN_SEND_TABLE( CPortalGameRulesProxy, DT_PortalGameRulesProxy )
		SendPropDataTable( "portal_gamerules_data", 0, &REFERENCE_SEND_TABLE( DT_PortalGameRules ), SendProxy_PortalGameRules )
	END_SEND_TABLE()
#endif

extern ConVar	sv_robust_explosions;
extern ConVar	sk_allow_autoaim;
extern ConVar	sk_autoaim_scale1;
extern ConVar	sk_autoaim_scale2;

#if !defined ( CLIENT_DLL )
extern ConVar	sv_alternateticks;
#endif // !CLIENT_DLL

#define PORTAL_WEIGHT_BOX_MODEL_NAME "models/props/metal_box.mdl"

#ifndef CLIENT_DLL
void CC_Create_PortalWeightBox( const CCommand &args )
{
	MDLCACHE_CRITICAL_SECTION();

	bool allowPrecache = CBaseEntity::IsPrecacheAllowed();
	CBaseEntity::SetAllowPrecache( true );

	CBaseEntity *entity = dynamic_cast< CBaseEntity * >( CreateEntityByName("prop_physics") );
	if (entity)
	{
		entity->PrecacheModel( PORTAL_WEIGHT_BOX_MODEL_NAME );
		entity->SetModel( PORTAL_WEIGHT_BOX_MODEL_NAME );
		entity->SetName( MAKE_STRING("box") );
		entity->AddSpawnFlags( SF_PHYSPROP_ENABLE_PICKUP_OUTPUT );
		entity->Precache();
		DispatchSpawn(entity);

		CBasePlayer* pPlayer = UTIL_GetCommandClient();
		if ( pPlayer )
		{
			trace_t tr;
			Vector forward;
			pPlayer->EyeVectors( &forward );
			UTIL_TraceLine(pPlayer->EyePosition(),
				pPlayer->EyePosition() + forward * MAX_TRACE_LENGTH, MASK_SOLID, 
				pPlayer, COLLISION_GROUP_NONE, &tr );
			if ( tr.fraction != 1.0 )
			{
				tr.endpos.z += 12;
				entity->Teleport( &tr.endpos, NULL, NULL );
				UTIL_DropToFloor( entity, MASK_SOLID );
			}
		}
	}
	CBaseEntity::SetAllowPrecache( allowPrecache );
}
static ConCommand ent_create_portal_weight_box("ent_create_portal_weight_box", CC_Create_PortalWeightBox, "Creates a weight box used in portal puzzles.", FCVAR_GAMEDLL | FCVAR_CHEAT);
#endif // CLIENT_DLL

#define PORTAL_METAL_SPHERE_MODEL_NAME "models/props/sphere.mdl"

#ifndef CLIENT_DLL
void CC_Create_PortalMetalSphere( const CCommand &args )
{
	MDLCACHE_CRITICAL_SECTION();

	bool allowPrecache = CBaseEntity::IsPrecacheAllowed();
	CBaseEntity::SetAllowPrecache( true );

	CBaseEntity *entity = dynamic_cast< CBaseEntity * >( CreateEntityByName("prop_physics") );
	if (entity)
	{
		entity->PrecacheModel( PORTAL_METAL_SPHERE_MODEL_NAME );
		entity->SetModel( PORTAL_METAL_SPHERE_MODEL_NAME );
		entity->SetName( MAKE_STRING("sphere") );
		entity->AddSpawnFlags( SF_PHYSPROP_ENABLE_PICKUP_OUTPUT );
		entity->Precache();
		DispatchSpawn(entity);

		CBasePlayer* pPlayer = UTIL_GetCommandClient();
		if ( pPlayer )
		{
			trace_t tr;
			Vector forward;
			pPlayer->EyeVectors( &forward );
			UTIL_TraceLine(pPlayer->EyePosition(),
				pPlayer->EyePosition() + forward * MAX_TRACE_LENGTH, MASK_SOLID, 
				pPlayer, COLLISION_GROUP_NONE, &tr );
			if ( tr.fraction != 1.0 )
			{
				tr.endpos.z += 12;
				entity->Teleport( &tr.endpos, NULL, NULL );
				UTIL_DropToFloor( entity, MASK_SOLID );
			}
		}
	}
	CBaseEntity::SetAllowPrecache( allowPrecache );
}
static ConCommand ent_create_portal_metal_sphere("ent_create_portal_metal_sphere", CC_Create_PortalMetalSphere, "Creates a reflective metal sphere.", FCVAR_GAMEDLL | FCVAR_CHEAT);
#endif // CLIENT_DLL

#ifdef CLIENT_DLL
#else

	extern bool		g_fGameOver;
	
	CPortalGameRules::CPortalGameRules()
	{
		m_bMegaPhysgun = false;
		ConVarRef sv_maxreplay("sv_maxreplay");
		if ( sv_maxreplay.IsValid() )
		{
			sv_maxreplay.SetValue( "1.5" );
		}
	}

	bool CPortalGameRules::ClientCommand( CBaseEntity *pEdict, const CCommand &args )
	{
		if( BaseClass::ClientCommand( pEdict, args ) )
			return true;

		CPortal_Player *pPlayer = (CPortal_Player *) pEdict;
		if ( pPlayer && pPlayer->ClientCommand( args ) )
			return true;

		return false;
	}

	void CPortalGameRules::PlayerSpawn( CBasePlayer *pPlayer )
	{
	}

	void CPortalGameRules::InitDefaultAIRelationships( void )
	{
		int i, j;

		CBaseCombatCharacter::AllocateDefaultRelationships();

		for (i=0; i<NUM_AI_CLASSES; i++)
		{
			for (j=0; j<NUM_AI_CLASSES; j++)
			{
				CBaseCombatCharacter::SetDefaultRelationship( (Class_T)i, (Class_T)j, D_NU, 0 );
			}
		}

		// Инициализация базовых отношений (классы HL2 / Portal)
		CBaseCombatCharacter::SetDefaultRelationship(CLASS_ANTLION, CLASS_PLAYER, D_HT, 0);
		CBaseCombatCharacter::SetDefaultRelationship(CLASS_BARNACLE, CLASS_PLAYER, D_HT, 0);
		CBaseCombatCharacter::SetDefaultRelationship(CLASS_COMBINE, CLASS_PLAYER, D_HT, 0);
		CBaseCombatCharacter::SetDefaultRelationship(CLASS_METROPOLICE, CLASS_PLAYER, D_HT, 0);
		CBaseCombatCharacter::SetDefaultRelationship(CLASS_HEADCRAB, CLASS_PLAYER, D_HT, 0);
		CBaseCombatCharacter::SetDefaultRelationship(CLASS_ZOMBIE, CLASS_PLAYER, D_HT, 0);
		CBaseCombatCharacter::SetDefaultRelationship(CLASS_VORTIGAUNT, CLASS_PLAYER, D_LI, 0);
		CBaseCombatCharacter::SetDefaultRelationship(CLASS_PLAYER, CLASS_PLAYER, D_NU, 0);
	}

#endif

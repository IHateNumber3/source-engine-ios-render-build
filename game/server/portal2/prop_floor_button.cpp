//========= Written from scratch against public Valve Developer Wiki docs =========
//
// prop_floor_button: a model entity that gets pressed by weight (player or a
// prop_weighted_cube) standing on it, and fires OnPressed/OnUnPressed.
//
// Documented behavior this implements (see developer.valvesoftware.com):
//   - Keyvalue: SuppressAnimSounds <bool>
//   - Inputs:   PressIn, PressOut
//   - Outputs:  OnPressed, OnUnPressed
//   - Detection: wiki explicitly says a trigger_multiple-style volume is the
//     documented substitute for the internal trigger_portal_button (which
//     can't be placed directly in a map). We build our own small
//     CBaseTrigger-derived volume the same way any custom trigger_multiple
//     works in stock Source SDK 2013.
//
// NOT implemented (retail-only extras, no public documentation covers their
// exact internal behavior, so left out rather than guessed at):
//   - Co-op player-color outputs (OnPressedBlue/OnPressedOrange)
//   - Achievement hooks
//   - Ball-only / cube-only button subclasses (prop_floor_ball_button etc.)
//     -- easy to add later as a CubeType check once prop_weighted_cube
//     exposes GetCubeType() the same way, but skipped for this first pass.
//
//===================================================================================

#include "cbase.h"
#include "props.h"
#include "triggers.h"
#include "tier0/memdbgon.h" // must be last include

#define PROP_FLOOR_BUTTON_MODEL_NAME "models/props/portal_button.mdl"

class CPropFloorButton;

//-----------------------------------------------------------------------------
// The detection volume. Same role the wiki says trigger_multiple plays for
// this entity type -- a plain touch-trigger that tells the button when
// something is standing on it.
//-----------------------------------------------------------------------------
class CFloorButtonTrigger : public CBaseTrigger
{
public:
	DECLARE_CLASS( CFloorButtonTrigger, CBaseTrigger );

	static CFloorButtonTrigger *Create( const Vector &vecOrigin, const Vector &vecMins, const Vector &vecMaxs, CPropFloorButton *pOwner );

	void Spawn( void );
	virtual bool PassesTriggerFilters( CBaseEntity *pOther );
	virtual void OnStartTouchAll( CBaseEntity *pOther );
	virtual void OnEndTouchAll( CBaseEntity *pOther );

private:
	CPropFloorButton *m_pOwner;
};

LINK_ENTITY_TO_CLASS( trigger_floor_button, CFloorButtonTrigger );

//-----------------------------------------------------------------------------
// The button itself.
//-----------------------------------------------------------------------------
class CPropFloorButton : public CDynamicProp
{
public:
	DECLARE_CLASS( CPropFloorButton, CDynamicProp );
	DECLARE_DATADESC();

	CPropFloorButton();

	virtual void Precache( void );
	virtual void Spawn( void );
	virtual void Activate( void );
	virtual void UpdateOnRemove( void );

	void Press( CBaseEntity *pActivator );
	void UnPress( CBaseEntity *pActivator );

	bool IsPressed( void ) const { return m_bButtonState; }

private:
	void InputPressIn( inputdata_t &inputdata );
	void InputPressOut( inputdata_t &inputdata );

	int		m_UpSequence;
	int		m_DownSequence;
	bool	m_bSuppressAnimSounds;

	CNetworkVar( bool, m_bButtonState );

	COutputEvent	m_OnPressed;
	COutputEvent	m_OnUnPressed;

	CHandle<CFloorButtonTrigger>	m_hTrigger;
};

LINK_ENTITY_TO_CLASS( prop_floor_button, CPropFloorButton );

BEGIN_DATADESC( CPropFloorButton )
	DEFINE_KEYFIELD( m_bSuppressAnimSounds, FIELD_BOOLEAN, "SuppressAnimSounds" ),
	DEFINE_FIELD( m_UpSequence, FIELD_INTEGER ),
	DEFINE_FIELD( m_DownSequence, FIELD_INTEGER ),
	DEFINE_FIELD( m_hTrigger, FIELD_EHANDLE ),

	DEFINE_INPUTFUNC( FIELD_VOID, "PressIn", InputPressIn ),
	DEFINE_INPUTFUNC( FIELD_VOID, "PressOut", InputPressOut ),

	DEFINE_OUTPUT( m_OnPressed, "OnPressed" ),
	DEFINE_OUTPUT( m_OnUnPressed, "OnUnPressed" ),
END_DATADESC()

CPropFloorButton::CPropFloorButton()
	: m_bButtonState( false )
	, m_bSuppressAnimSounds( false )
{
}

void CPropFloorButton::Precache( void )
{
	PrecacheModel( PROP_FLOOR_BUTTON_MODEL_NAME );
	BaseClass::Precache();
}

void CPropFloorButton::Spawn( void )
{
	KeyValue( "model", PROP_FLOOR_BUTTON_MODEL_NAME );

	Precache();
	BaseClass::Spawn();

	SetSolid( SOLID_VPHYSICS );

	m_UpSequence = LookupSequence( "up" );
	m_DownSequence = LookupSequence( "down" );
	if ( m_UpSequence >= 0 )
	{
		ResetSequence( m_UpSequence );
	}

	CreateVPhysics();
}

void CPropFloorButton::Activate( void )
{
	BaseClass::Activate();

	// Build the detection volume once the rest of the map has spawned.
	Vector vecMins( -20, -20, 0 );
	Vector vecMaxs( 20, 20, 14 );
	m_hTrigger = CFloorButtonTrigger::Create( GetAbsOrigin(), vecMins, vecMaxs, this );
}

void CPropFloorButton::UpdateOnRemove( void )
{
	if ( m_hTrigger )
	{
		UTIL_Remove( m_hTrigger );
		m_hTrigger = NULL;
	}
	BaseClass::UpdateOnRemove();
}

void CPropFloorButton::Press( CBaseEntity *pActivator )
{
	if ( m_bButtonState )
		return; // already pressed, nothing to do

	m_bButtonState = true;

	if ( m_DownSequence >= 0 )
	{
		ResetSequence( m_DownSequence );
	}

	if ( !m_bSuppressAnimSounds )
	{
		EmitSound( "Portal.button_down" );
	}

	m_OnPressed.FireOutput( pActivator, this );
}

void CPropFloorButton::UnPress( CBaseEntity *pActivator )
{
	if ( !m_bButtonState )
		return; // already unpressed, nothing to do

	m_bButtonState = false;

	if ( m_UpSequence >= 0 )
	{
		ResetSequence( m_UpSequence );
	}

	if ( !m_bSuppressAnimSounds )
	{
		EmitSound( "Portal.button_up" );
	}

	m_OnUnPressed.FireOutput( pActivator, this );
}

void CPropFloorButton::InputPressIn( inputdata_t &inputdata )
{
	Press( inputdata.pActivator );
}

void CPropFloorButton::InputPressOut( inputdata_t &inputdata )
{
	UnPress( inputdata.pActivator );
}

//-----------------------------------------------------------------------------
// Trigger volume implementation
//-----------------------------------------------------------------------------
CFloorButtonTrigger *CFloorButtonTrigger::Create( const Vector &vecOrigin, const Vector &vecMins, const Vector &vecMaxs, CPropFloorButton *pOwner )
{
	CFloorButtonTrigger *pTrigger = (CFloorButtonTrigger *)CreateEntityByName( "trigger_floor_button" );
	if ( !pTrigger )
		return NULL;

	UTIL_SetOrigin( pTrigger, vecOrigin );
	UTIL_SetSize( pTrigger, vecMins, vecMaxs );

	DispatchSpawn( pTrigger );

	pTrigger->SetParent( pOwner );
	pTrigger->m_pOwner = pOwner;

	return pTrigger;
}

void CFloorButtonTrigger::Spawn( void )
{
	SetMoveType( MOVETYPE_NONE );
	SetSolid( SOLID_BSP );
	AddSolidFlags( FSOLID_NOT_SOLID | FSOLID_TRIGGER );

	BaseClass::Spawn();
}

bool CFloorButtonTrigger::PassesTriggerFilters( CBaseEntity *pOther )
{
	if ( !BaseClass::PassesTriggerFilters( pOther ) )
		return false;

	// Players always count.
	if ( pOther->IsPlayer() )
		return true;

	// Anything with weight (physics-simulated) also counts -- this covers
	// prop_weighted_cube and any ordinary physics prop pushed onto it,
	// same as the documented "activated by a player or objects" behavior.
	if ( pOther->VPhysicsGetObject() != NULL )
		return true;

	return false;
}

void CFloorButtonTrigger::OnStartTouchAll( CBaseEntity *pOther )
{
	if ( m_pOwner )
	{
		m_pOwner->Press( pOther );
	}
	BaseClass::OnStartTouchAll( pOther );
}

void CFloorButtonTrigger::OnEndTouchAll( CBaseEntity *pOther )
{
	if ( m_pOwner )
	{
		m_pOwner->UnPress( pOther );
	}
	BaseClass::OnEndTouchAll( pOther );
}

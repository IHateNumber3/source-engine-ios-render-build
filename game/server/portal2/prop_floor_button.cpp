//========= prop_floor_button -- clean rewrite =========
//
// A model entity pressed by weight (player or prop_weighted_cube), firing
// OnPressed/OnUnPressed. Detection uses a plain trigger volume, same as a
// standard trigger_multiple (which is how Portal 1 handled floor button
// triggers too) -- a separate CBaseTrigger-derived entity parented to the
// button model.
//
// Documented keyvalue/input/output names (Valve Developer Wiki):
//   Keyvalue: SuppressAnimSounds <bool>
//   Inputs:   PressIn, PressOut
//   Outputs:  OnPressed, OnUnPressed
//
// Model: models/props/portal_button.mdl (confirmed correct against a real
// legally-owned copy). Sequences on that model: BindPose, up, idledown,
// down -- BindPose is the natural idle/unpressed look (no separate idle-up
// sequence exists), "down"/"up" are press/release transitions only.
// Sound events baked into the model: Portal.ButtonDepress / Portal.ButtonRelease.
//
//========================================================

#include "cbase.h"
#include "props.h"
#include "triggers.h"
#include "prop_weighted_cube.h"
#include "tier0/memdbgon.h" // must be last include

#define PROP_FLOOR_BUTTON_MODEL_NAME "models/props/portal_button.mdl"

class CPropFloorButton;

//-----------------------------------------------------------------------------
// Detection volume -- plain trigger_multiple-style touch trigger.
//-----------------------------------------------------------------------------
class CFloorButtonTrigger : public CBaseTrigger
{
public:
	DECLARE_CLASS( CFloorButtonTrigger, CBaseTrigger );

	static CFloorButtonTrigger *Create( const Vector &vecMins, const Vector &vecMaxs, CPropFloorButton *pOwner );

	void Spawn( void );
	virtual bool PassesTriggerFilters( CBaseEntity *pOther );
	virtual void StartTouch( CBaseEntity *pOther );
	virtual void EndTouch( CBaseEntity *pOther );

	CPropFloorButton *m_pOwner;
};

LINK_ENTITY_TO_CLASS( trigger_floor_button, CFloorButtonTrigger );

//-----------------------------------------------------------------------------
// The button.
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
	void AnimThink( void );

	bool IsPressed( void ) const { return m_bButtonState; }

private:
	void InputPressIn( inputdata_t &inputdata );
	void InputPressOut( inputdata_t &inputdata );

	int		m_UpSequence;
	int		m_DownSequence;
	bool	m_bSuppressAnimSounds;
	bool	m_bButtonState;

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
	DEFINE_FIELD( m_bButtonState, FIELD_BOOLEAN ),

	DEFINE_INPUTFUNC( FIELD_VOID, "PressIn", InputPressIn ),
	DEFINE_INPUTFUNC( FIELD_VOID, "PressOut", InputPressOut ),

	DEFINE_OUTPUT( m_OnPressed, "OnPressed" ),
	DEFINE_OUTPUT( m_OnUnPressed, "OnUnPressed" ),
END_DATADESC()

CPropFloorButton::CPropFloorButton()
	: m_bButtonState( false )
	, m_bSuppressAnimSounds( false )
	, m_UpSequence( -1 )
	, m_DownSequence( -1 )
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

	m_UpSequence = LookupSequence( "up" );
	m_DownSequence = LookupSequence( "down" );
	// Deliberately not forcing any sequence at idle -- BindPose (the
	// model's default) is the correct unpressed look. "up"/"down" only
	// play as transitions inside Press()/UnPress().

	// SOLID_VPHYSICS needs a real physics object behind it, or the button
	// has no collision at all.
	SetSolid( SOLID_VPHYSICS );
	CreateVPhysics();

	// Collision yes, movement no -- it shouldn't get pushed/knocked around.
	IPhysicsObject *pButtonPhys = VPhysicsGetObject();
	if ( pButtonPhys )
	{
		pButtonPhys->EnableMotion( false );
	}

	// Manually drive the animation forward every server frame so the
	// press/release transitions actually play through rather than sitting
	// on frame 0.
	SetThink( &CPropFloorButton::AnimThink );
	SetNextThink( gpGlobals->curtime );
}

void CPropFloorButton::AnimThink( void )
{
	StudioFrameAdvance();
	SetNextThink( gpGlobals->curtime );
}

void CPropFloorButton::Activate( void )
{
	BaseClass::Activate();

	Vector vecMins( -20, -20, 0 );
	Vector vecMaxs( 20, 20, 14 );
	m_hTrigger = CFloorButtonTrigger::Create( vecMins, vecMaxs, this );
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
		return;

	m_bButtonState = true;
	m_nSkin = 1;

	if ( m_DownSequence >= 0 )
	{
		SetSequence( m_DownSequence );
		SetCycle( 0.0f );
		SetPlaybackRate( 1.0f );
		m_flAnimTime = gpGlobals->curtime;
		UseClientSideAnimation();
	}

	if ( !m_bSuppressAnimSounds )
	{
		EmitSound( "Portal.ButtonDepress" );
	}

	m_OnPressed.FireOutput( pActivator, this );
}

void CPropFloorButton::UnPress( CBaseEntity *pActivator )
{
	if ( !m_bButtonState )
		return;

	m_bButtonState = false;
	m_nSkin = 0;

	if ( m_UpSequence >= 0 )
	{
		SetSequence( m_UpSequence );
		SetCycle( 0.0f );
		SetPlaybackRate( 1.0f );
		m_flAnimTime = gpGlobals->curtime;
		UseClientSideAnimation();
	}

	if ( !m_bSuppressAnimSounds )
	{
		EmitSound( "Portal.ButtonRelease" );
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
// Trigger volume
//-----------------------------------------------------------------------------
CFloorButtonTrigger *CFloorButtonTrigger::Create( const Vector &vecMins, const Vector &vecMaxs, CPropFloorButton *pOwner )
{
	CFloorButtonTrigger *pTrigger = (CFloorButtonTrigger *)CreateEntityByName( "trigger_floor_button" );
	if ( !pTrigger )
		return NULL;

	pTrigger->SetParent( pOwner );
	pTrigger->SetLocalOrigin( vec3_origin );
	UTIL_SetSize( pTrigger, vecMins, vecMaxs );

	DispatchSpawn( pTrigger );

	pTrigger->m_pOwner = pOwner;

	return pTrigger;
}

void CFloorButtonTrigger::Spawn( void )
{
	BaseClass::Spawn();

	// Set these AFTER BaseClass::Spawn() so they're the final word --
	// otherwise the volume can end up solid (blocking movement like a
	// wall) instead of a pure pass-through trigger.
	SetMoveType( MOVETYPE_NONE );
	SetSolid( SOLID_BSP );
	SetSolidFlags( FSOLID_NOT_SOLID | FSOLID_TRIGGER );
}

bool CFloorButtonTrigger::PassesTriggerFilters( CBaseEntity *pOther )
{
	if ( !BaseClass::PassesTriggerFilters( pOther ) )
		return false;

	if ( pOther->IsPlayer() )
		return true;

	if ( FClassnameIs( pOther, "prop_weighted_cube" ) )
	{
		CPropWeightedCube *pCube = dynamic_cast<CPropWeightedCube*>( pOther );
		if ( pCube )
		{
			WeightedCubeType_e type = pCube->GetCubeType();
			return ( type == CUBE_STANDARD || type == CUBE_COMPANION || type == CUBE_REFLECTIVE );
		}
	}

	if ( pOther->VPhysicsGetObject() != NULL )
		return true;

	return false;
}

void CFloorButtonTrigger::StartTouch( CBaseEntity *pOther )
{
	BaseClass::StartTouch( pOther );

	if ( !PassesTriggerFilters( pOther ) )
		return;

	if ( m_pOwner )
	{
		m_pOwner->Press( pOther );
	}
}

void CFloorButtonTrigger::EndTouch( CBaseEntity *pOther )
{
	if ( m_pOwner && PassesTriggerFilters( pOther ) )
	{
		m_pOwner->UnPress( pOther );
	}

	BaseClass::EndTouch( pOther );
}

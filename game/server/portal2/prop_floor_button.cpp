//========= prop_floor_button -- simplified, no separate trigger entity =========
//
// Touch detection happens directly on the button model's own solid physics
// collision (SOLID_VPHYSICS) via StartTouch/EndTouch, instead of a separate
// child trigger entity. Standing on any solid object generates touch
// callbacks in Source, same mechanism as ground-entity touch -- this avoids
// the whole class of bugs around trigger creation timing (Activate() not
// running for ent_create-spawned entities), parenting/origin ordering, and
// solid-flag clobbering that a separate trigger volume kept hitting.
//
// Model: models/props/portal_button.mdl (confirmed correct).
// Sequences: BindPose (idle/unpressed default), up/down (press/release
// transitions only). Sounds: Portal.ButtonDepress / Portal.ButtonRelease.
//
//================================================================================

#include "cbase.h"
#include "props.h"
#include "prop_weighted_cube.h"
#include "tier0/memdbgon.h" // must be last include

#define PROP_FLOOR_BUTTON_MODEL_NAME "models/props/portal_button.mdl"

class CPropFloorButton : public CDynamicProp
{
public:
	DECLARE_CLASS( CPropFloorButton, CDynamicProp );
	DECLARE_DATADESC();

	CPropFloorButton();

	virtual void Precache( void );
	virtual void Spawn( void );
	virtual void StartTouch( CBaseEntity *pOther );
	virtual void EndTouch( CBaseEntity *pOther );

	void Press( CBaseEntity *pActivator );
	void UnPress( CBaseEntity *pActivator );
	void AnimThink( void );

	bool IsPressed( void ) const { return m_bButtonState; }

private:
	void InputPressIn( inputdata_t &inputdata );
	void InputPressOut( inputdata_t &inputdata );
	bool ShouldReactTo( CBaseEntity *pOther );

	int		m_UpSequence;
	int		m_DownSequence;
	bool	m_bSuppressAnimSounds;
	bool	m_bButtonState;
	int		m_nTouchers; // how many valid touchers are currently on it

	COutputEvent	m_OnPressed;
	COutputEvent	m_OnUnPressed;
};

LINK_ENTITY_TO_CLASS( prop_floor_button, CPropFloorButton );

BEGIN_DATADESC( CPropFloorButton )
	DEFINE_KEYFIELD( m_bSuppressAnimSounds, FIELD_BOOLEAN, "SuppressAnimSounds" ),
	DEFINE_FIELD( m_UpSequence, FIELD_INTEGER ),
	DEFINE_FIELD( m_DownSequence, FIELD_INTEGER ),
	DEFINE_FIELD( m_bButtonState, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_nTouchers, FIELD_INTEGER ),

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
	, m_nTouchers( 0 )
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
	// Not forcing any sequence at idle -- BindPose is the correct
	// unpressed look; up/down only play as transitions in Press/UnPress.

	SetSolid( SOLID_VPHYSICS );
	CreateVPhysics();
	SetMoveType( MOVETYPE_NONE ); // never moved by gravity/physics/pushing

	IPhysicsObject *pButtonPhys = VPhysicsGetObject();
	if ( pButtonPhys )
	{
		pButtonPhys->EnableMotion( false );
	}
	else
	{
		Warning( "prop_floor_button: CreateVPhysics() failed -- collision may be wrong\n" );
	}

	// Solid objects need FSOLID_TRIGGER-style touch to actually fire touch
	// events for things resting on/pushing into them in some engine builds
	// -- add it defensively alongside the real solid collision so touch
	// callbacks are guaranteed to fire even if the stock solid-object touch
	// path doesn't in this fork.
	AddSolidFlags( FSOLID_TRIGGER );

	SetThink( &CPropFloorButton::AnimThink );
	SetNextThink( gpGlobals->curtime );
}

void CPropFloorButton::AnimThink( void )
{
	StudioFrameAdvance();
	SetNextThink( gpGlobals->curtime );
}

bool CPropFloorButton::ShouldReactTo( CBaseEntity *pOther )
{
	if ( !pOther )
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

void CPropFloorButton::StartTouch( CBaseEntity *pOther )
{
	BaseClass::StartTouch( pOther );

	if ( !ShouldReactTo( pOther ) )
		return;

	m_nTouchers++;
	if ( m_nTouchers == 1 )
	{
		Press( pOther );
	}
}

void CPropFloorButton::EndTouch( CBaseEntity *pOther )
{
	if ( ShouldReactTo( pOther ) )
	{
		m_nTouchers--;
		if ( m_nTouchers <= 0 )
		{
			m_nTouchers = 0;
			UnPress( pOther );
		}
	}

	BaseClass::EndTouch( pOther );
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

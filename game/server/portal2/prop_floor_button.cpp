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
//     can't be placed directly in a map).
//   - CubeType filtering: only STANDARD/COMPANION/REFLECTIVE cubes press the
//     button (spheres etc. don't), via prop_weighted_cube's GetCubeType().
//
// NOT implemented: co-op team outputs, achievement hooks -- no public docs
// cover their exact internal behavior.
//
//===================================================================================

#include "cbase.h"
#include "props.h"
#include "triggers.h"
#include "prop_weighted_cube.h"
#include "tier0/memdbgon.h" // must be last include

// NOTE: unverified model path, see prop_weighted_cube.h/.cpp note. Check
// your own legal Portal 2 files and correct if this doesn't load.
#define PROP_FLOOR_BUTTON_MODEL_NAME "models/props/portal_button.mdl"

class CPropFloorButton;

//-----------------------------------------------------------------------------
// The detection volume.
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
		// Match the confirmed-working reference pattern exactly: plain
		// SetSequence() (not ResetSequence/ResetSequenceInfo, whose exact
		// side effects in this engine fork I was guessing at) + explicit
		// cycle/rate.
		SetSequence( m_UpSequence );
		SetCycle( 1.0f );
		SetPlaybackRate( 0.0f );
		UseClientSideAnimation();
	}

	// SOLID_VPHYSICS needs an actual physics object behind it or the entity
	// has no collision at all (earlier removal of this call was based on a
	// misdiagnosis of an unrelated prop's problem, not this button's).
	CreateVPhysics();

	// Lock the physics object in place -- it should have solid collision
	// but never actually move/get pushed around by the player or a cube
	// resting on it.
	IPhysicsObject *pButtonPhys = VPhysicsGetObject();
	if ( pButtonPhys )
	{
		pButtonPhys->EnableMotion( false );
	}
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

	if ( m_DownSequence >= 0 )
	{
		SetSequence( m_DownSequence );
		SetPlaybackRate( 1.0f );
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

	if ( m_UpSequence >= 0 )
	{
		SetSequence( m_UpSequence );
		SetPlaybackRate( 1.0f );
		UseClientSideAnimation();
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
CFloorButtonTrigger *CFloorButtonTrigger::Create( const Vector &vecMins, const Vector &vecMaxs, CPropFloorButton *pOwner )
{
	CFloorButtonTrigger *pTrigger = (CFloorButtonTrigger *)CreateEntityByName( "trigger_floor_button" );
	if ( !pTrigger )
		return NULL;

	// Parent first, then zero the local origin -- setting an absolute world
	// origin before parenting and letting SetParent re-base it into local
	// space was making the trigger end up offset from the button model.
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

	// Set these AFTER BaseClass::Spawn() so they're the final word -- if
	// they were set before, whatever CBaseTrigger::Spawn() does internally
	// could clobber them, leaving the volume solid (blocking movement like
	// a wall) instead of a pure pass-through trigger.
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
			// Spheres don't press floor buttons; standard/companion/
			// reflective cubes do.
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
